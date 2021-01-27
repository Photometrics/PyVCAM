/* System */
#include <string>
#include <mutex>

/* Local */
#include "FpsLimiter.h"
#include "Acquisition.h"
#include "Camera.h"
#include "Settings.h"

class Helper final
{
public:
	Helper();
	~Helper();

	bool m_userAbortFlag = false;

	bool InstallTerminationHandlers();
	bool AttachCamera(std::string camName); // Attach/connect camera

public: // Settings access
	bool SetAcqMode(pm::AcqMode value);
	bool SetAcqFrameCount(uns32 value);
	bool SetExposure(uns32 value);
	bool SetRegions(const std::vector<rgn_type> &value);
	bool SetStorageType(pm::StorageType value);
	bool SetMaxStackSize(size_t value);
	bool SetSaveDir(const std::string &value);

public: // Acquisition functions
	bool StartAcquisition(); // Start the acquisition
	bool JoinAcquisition(); // Wait for acquisition to finish
	bool AcquisitionStatus(); // Return true if acquisition is active, false otherwise
	bool AcquisitionStats(double& acqFps, size_t& acqFramesValid,
        size_t& acqFramesLost, size_t& acqFramesMax, size_t& acqFramesCached,
		double& diskFps, size_t& diskFramesValid,
        size_t& diskFramesLost, size_t& diskFramesMax, size_t& diskFramesCached); // Get acquisition stats
	void AbortAcquisition(bool force); // Abort any running acquisition
	void InputTimerTick(); // Input FPS limiter timer tick
	bool GetFrameData(void** data, uns32* frameBytes, uns32* frameNum, uns16* frameW, uns16* frameH);

private:
    void OnFpsLimiterEvent(std::shared_ptr<pm::Frame> frame);

private:
	bool InitAcquisition();
	void UninitAcquisition();

	bool acq_ready = false;
	bool acq_active = false;

	pm::Settings m_settings;
	unsigned int m_targetFps; // Not stored in Settings
	std::shared_ptr<pm::Camera> m_camera;
	std::shared_ptr<pm::Acquisition> m_acquisition;
	std::shared_ptr<pm::FpsLimiter> m_fpslimiter;
	std::shared_ptr<pm::Frame> m_frame;
	std::mutex m_frameMutex;
};