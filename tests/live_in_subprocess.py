import argparse
import multiprocessing as mp
import time
import traceback

import numpy as np
import cv2


def camera_worker(frame_conn, stop_event, status_queue, exp_time_ms: int,
                  cam_index: int, roi_width: int, roi_height: int):
    try:
        # Import PyVCAM here to keep the main process free from its state.
        from pyvcam import pvc  # pylint: disable=import-outside-toplevel
        from pyvcam.camera import Camera  # pylint: disable=import-outside-toplevel
        from pyvcam import constants as const  # pylint: disable=import-outside-toplevel

        pvc.init_pvcam()
        status_queue.put("PVCAM initialized")

        cams = list(Camera.detect_camera())
        if not cams:
            status_queue.put("ERROR: no cameras detected")
            return
        if cam_index < 0 or cam_index >= len(cams):
            status_queue.put(
                f"ERROR: cam_index {cam_index} out of range (0..{len(cams)-1})")
            return

        cam = cams[cam_index]
        cam.open()
        status_queue.put(f"Camera {cam_index}: {cam.name}")

        if roi_width < 1 or roi_width > cam.sensor_size[0]:
            status_queue.put(
                f"ERROR: roi_width {roi_width} out of range (1..{cam.sensor_size[0]})")
            return
        if roi_height < 1 or roi_height > cam.sensor_size[1]:
            status_queue.put(
                f"ERROR: roi_height {roi_height} out of range (1..{cam.sensor_size[1]})")
            return

        bit_depth = cam.bit_depth_host
        bytes_per_pixel = (bit_depth + 7) // 8
        roi_bytes = bytes_per_pixel * roi_height * roi_width

        s1 = (cam.sensor_size[0] - roi_width) // 2
        p1 = (cam.sensor_size[1] - roi_height) // 2
        cam.set_roi(s1, p1, roi_width, roi_height)
        status_queue.put(f"ROI: {roi_width}x{roi_height} @({s1},{p1})")

        # cam.readout_port = 2
        # cam.speed = 0
        # cam.gain = 1

        cam.set_param(const.PARAM_METADATA_ENABLED, True)

        cam.start_live(exp_time=exp_time_ms)
        status_queue.put(f"Acquisition started - {exp_time_ms} ms exp. time")

        cnt = 0
        dropped = 0
        t_start = time.time()

        # Main capture loop
        while not stop_event.is_set():
            frame, fps, frame_count = cam.poll_frame()

            cnt += 1
            if cnt < frame_count:
                if cnt + 1 == frame_count:
                    status_queue.put(f'Dropped frame {cnt}')
                else:
                    status_queue.put(f'Dropped frames {cnt}-{frame_count - 1}')
                dropped += frame_count - cnt
                cnt = frame_count
            status_queue.put(f'Frames: {frame_count}\tFrame Rate: {fps:.1f}'
                             f'\tNr.: {frame["meta_data"]["frame_header"]["frameNr"]}')

            frame_data = frame['pixel_data'].tobytes()

            # Send the data over the pipe. Use send_bytes for raw transfer.
            try:
                frame_conn.send_bytes(frame_data, 0, roi_bytes)
            except (BrokenPipeError, EOFError):
                status_queue.put("ERROR: parent pipe closed")
                break

        t_end = time.time()
        fps_avg = cnt / (t_end - t_start)
        status_queue.put(
            f'Total frames: {cnt}, dropped: {dropped}, average fps: {fps_avg:.1f}')

        # Stop acquisition
        try:
            cam.finish()
            status_queue.put("Acquisition stopped")
        except Exception:  # pylint: disable=broad-except
            pass

        # Cleanup
        try:
            cam.close()
            status_queue.put("Camera closed")
        except Exception:  # pylint: disable=broad-except
            pass

    except Exception as e:  # pylint: disable=broad-except
        # send traceback to parent for debugging
        tb = traceback.format_exc()
        try:
            status_queue.put(f"Exception: {str(e)}")
            status_queue.put(tb)
        except Exception:  # pylint: disable=broad-except
            pass
    finally:
        try:
            pvc.uninit_pvcam()
            status_queue.put("PVCAM uninitialized")
        except Exception:  # pylint: disable=broad-except
            pass


def main():
    parser = argparse.ArgumentParser(
        description="PyVCAM live viewer with subprocess-isolated camera I/O")
    parser.add_argument('--exp-time', type=int, default=20,
                        help='Exposure time (ms) for each frame')
    parser.add_argument('--cam-index', type=int, default=0,
                        help='Index of camera to open (0..N)')
    parser.add_argument('--roi-width', type=int, default=2000,
                        help='ROI width (px)')
    parser.add_argument('--roi-height', type=int, default=1500,
                        help='ROI height (px)')
    args = parser.parse_args()

    # ENSURE THIS MATCHES CAMERA SETTINGS
    bit_depth = 12  # cam.bit_depth_host

    bytes_per_pixel = (bit_depth + 7) // 8
    max_pix_value = 2 ** (bytes_per_pixel * 8) - 1
    roi_bytes = bytes_per_pixel * args.roi_width * args.roi_height
    dtype = np.uint16 if bytes_per_pixel == 2 else (
        np.uint8 if bytes_per_pixel == 1 else np.uint32)

    width = 800
    height = (args.roi_height * width) // args.roi_width
    dim = (width, height)

    # One-way pipe for frame bytes: child -> parent
    parent_conn, child_conn = mp.Pipe(duplex=False)
    stop_event = mp.Event()
    status_queue = mp.Queue()

    proc = mp.Process(target=camera_worker,
                      args=(child_conn,
                            stop_event,
                            status_queue,
                            args.exp_time,
                            args.cam_index,
                            args.roi_width,
                            args.roi_height),
                      daemon=True)
    proc.start()

    try:
        while True:
            # Read status messages (non-blocking)
            while not status_queue.empty():
                msg = status_queue.get_nowait()
                print("[camera status]", msg)

            # Wait for a frame to arrive; poll with a short timeout
            if parent_conn.poll(1.0):
                try:
                    frame_data = parent_conn.recv_bytes(roi_bytes)
                except (EOFError, BrokenPipeError):
                    print("Parent: frame pipe closed by child")
                    break

                # Decode
                frame = np.frombuffer(frame_data, dtype=dtype)
                frame = frame.reshape((args.roi_height, args.roi_width))
                # Shrink to smaller window
                disp_img = cv2.resize(frame, dim, interpolation=cv2.INTER_AREA)
                # Normalize to min-max range to make even background noise visible
                disp_img = cv2.normalize(disp_img, dst=None,
                                         alpha=0, beta=max_pix_value,
                                         norm_type=cv2.NORM_MINMAX)

                cv2.imshow('Live Mode (subproc)', disp_img)

            # Handle key presses
            key = cv2.waitKey(1) & 0xFF
            if key == ord('q') or key == 27:  # 'q' or ESC
                print("User requested exit")
                # signal child to stop
                stop_event.set()

            # if child died, break
            if not proc.is_alive():
                print("Child process exited")
                break

        # signal child to stop
        stop_event.set()

        # Read remaining status messages (non-blocking)
        while not status_queue.empty():
            msg = status_queue.get_nowait()
            print("[camera status]", msg)

    except KeyboardInterrupt:
        print("KeyboardInterrupt: shutting down")
        stop_event.set()

    finally:
        # give the child a moment to finish
        proc.join(timeout=5.0)
        if proc.is_alive():
            print("Child did not exit cleanly; terminating")
            proc.terminate()
            proc.join(timeout=1.0)

        try:
            parent_conn.close()
        except Exception:  # pylint: disable=broad-except
            pass
        try:
            status_queue.close()
        except Exception:  # pylint: disable=broad-except
            pass

        cv2.destroyAllWindows()


if __name__ == '__main__':
    main()
