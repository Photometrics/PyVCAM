from pyvcam import pvc 
from pyvcam.camera import Camera   
from pyvcam import constants as const
import time
import numpy as np

# Initialize test variables
test_exp_time = 10
test_num_frames = 10
test_ld_sw_value = 10

pvc.init_pvcam()                   # Initialize PVCAM 
cam = next(Camera.detect_camera()) # Use generator to find first camera. 
cam.open()                         # Open the camera.

# Check that PSM is available in the camera
if (cam.check_param(const.PARAM_SCAN_MODE)):
    # Get Auto Mode values

    # Set camera to HDR Gain 1
    print("Setting camera to HDR Gain 1")
    cam.speed_table_index = 0 
    cam.gain = 1

    ###############################################################################################################
    # Set to Auto Mode
    ###############################################################################################################
    if (cam.prog_scan_mode is not const.prog_scan_modes["Auto"]):
        print("\nSetting PSM to Auto")
        cam.prog_scan_mode = "Auto"
        
    assert cam.get_param(const.PARAM_SCAN_MODE, const.ATTR_CURRENT) is const.prog_scan_modes["Auto"], "Programmable scan mode wasn't set to Auto"
    
    # Get frames and time to sequence
    start = time.time()
    frames = cam.get_sequence(num_frames=test_num_frames,exp_time=test_exp_time)
    end = time.time()

    # Since numpy's mean can be inaccurate in single precision, computing in float64 instead, which is more accurate
    auto_mean_hdr_1 = np.mean(frames, dtype=np.float64)
    auto_seq_time_hdr_1 = end-start

    print("First five rows of frame: {}, {}, {}, {}, {}".format(*frames[0][:5]))
    print("Mean = {0} ADU".format(auto_mean_hdr_1))
    print("Time to capture sequence = {0}".format(auto_seq_time_hdr_1))

    #################################################################################################################
    # Set to Line Delay Mode
    #################################################################################################################
    if (cam.prog_scan_mode is not const.prog_scan_modes["Line Delay"]):
        print("\nSetting PSM to Line Delay")
        cam.prog_scan_mode = "Line Delay"
        
    assert cam.get_param(const.PARAM_SCAN_MODE, const.ATTR_CURRENT) is const.prog_scan_modes["Line Delay"], "Programmable scan mode wasn't set to line delay"
    
    # Set Line Delay Params
    print("Setting scan direction to UP and line delay to ",test_ld_sw_value)

    cam.set_param(const.PARAM_SCAN_DIRECTION, const.PL_SCAN_DIRECTION_UP)
    cam.set_param(const.PARAM_SCAN_LINE_DELAY, test_ld_sw_value)

    assert cam.get_param(const.PARAM_SCAN_DIRECTION, const.ATTR_CURRENT) is const.PL_SCAN_DIRECTION_UP, "Scan direction wasn't set to up"
    assert cam.get_param(const.PARAM_SCAN_LINE_DELAY, const.ATTR_CURRENT) is test_ld_sw_value

    # Print scan line time and scan width to check that they're being set automatically to the right values
    print("Param scan line time is automatically now {0}".format(cam.get_param(const.PARAM_SCAN_LINE_TIME, const.ATTR_CURRENT)))
    print("Param scan width is automatically now {0}".format(cam.get_param(const.PARAM_SCAN_WIDTH, const.ATTR_CURRENT)))
    
    # Get frames and time to sequence
    start = time.time()
    frames = cam.get_sequence(num_frames=test_num_frames,exp_time=test_exp_time)
    end = time.time()

    # Since numpy's mean can be inaccurate in single precision, computing in float64 instead, which is more accurate
    ld_mean = np.mean(frames, dtype=np.float64)
    ld_seq_time = end-start

    print("First five rows of frame: {}, {}, {}, {}, {}".format(*frames[0][:5]))
    print("Mean = {0} ADU".format(ld_mean))
    print("Time to sequence = {0}".format(ld_seq_time))

    assert ld_seq_time > auto_seq_time_hdr_1, "Auto Mode sequence time was greater than Line Delay sequence time (HDRG1)"

    ###################################################################################################
    # Set to Scan Width Mode
    ##################################################################################################
    if (cam.prog_scan_mode is not const.prog_scan_modes["Scan Width"]):
        print("\nSetting PSM to Scan Width")
        cam.prog_scan_mode = "Scan Width"

    assert cam.get_param(const.PARAM_SCAN_MODE, const.ATTR_CURRENT) is const.prog_scan_modes["Scan Width"], "Programmable scan mode wasn't set to Scan Width"
    
    # Set Scan Width Params
    print("Setting scan direction to UP and Scan Width to ", test_ld_sw_value)

    cam.set_param(const.PARAM_SCAN_DIRECTION, const.PL_SCAN_DIRECTION_UP)
    cam.set_param(const.PARAM_SCAN_WIDTH, test_ld_sw_value)

    assert cam.get_param(const.PARAM_SCAN_DIRECTION, const.ATTR_CURRENT) is const.PL_SCAN_DIRECTION_UP, "Scan direction wasn't set to UP" 
    assert cam.get_param(const.PARAM_SCAN_WIDTH, const.ATTR_CURRENT) is test_ld_sw_value

    print("Param scan line time is automatically now {0}".format(cam.get_param(const.PARAM_SCAN_LINE_TIME, const.ATTR_CURRENT)))
    print("Param line delay is automatically now {0}".format(cam.get_param(const.PARAM_SCAN_LINE_DELAY, const.ATTR_CURRENT)))
    
    # Get frames and time to sequence
    start = time.time()
    frames = cam.get_sequence(num_frames=test_num_frames,exp_time=test_exp_time)
    end = time.time()

    # Since numpy's mean can be inaccurate in single precision, computing in float64 instead, which is more accurate
    sw_mean = np.mean(frames, dtype=np.float64)
    sw_seq_time = end-start

    print("First five rows of frame: {}, {}, {}, {}, {}".format(*frames[0][:5]))
    print("Mean = {0} ADU".format(sw_mean))
    print("Time to sequence = {0}".format(sw_seq_time))

    assert sw_seq_time > auto_seq_time_hdr_1, "Auto Mode sequence time was greater than Scan Width sequence time (HDRG1)"

    
else:
    print("PSM isn't available")

cam.close()
pvc.uninit_pvcam()

print('Finished test')
