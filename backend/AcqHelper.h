/* System */
#include <string>

/* Local */
#include "FpsLimiter.h"
#include "Acquisition.h"
#include "OptionController.h"
#include "Camera.h"
#include "Settings.h"

class Helper final : private pm::IFpsLimiterListener
{
public:
	Helper();
	~Helper();

	bool m_userAbortFlag = false;

	bool InstallTerminationHandlers();
	bool ApplySettings(uns32 expTotal, uns32 expTime, int16 expMode, const std::vector<rgn_type>& regions, const char *path);
	void ShowSettings();
	bool AttachCamera(std::string camName); // Attach/connect camera
	bool StartAcquisition(); // Start the acquisition
	bool JoinAcquisition(); // Wait for acquisition to finish
	bool AcquisitionStatus(); // Return true if acquisition is active, false otherwise
	void AcquisitionStats(double& acqFps, size_t& acqFramesValid,
        size_t& acqFramesLost, size_t& acqFramesMax, size_t& acqFramesCached,
		double& diskFps, size_t& diskFramesValid,
        size_t& diskFramesLost, size_t& diskFramesMax, size_t& diskFramesCached); // Get acquisition stats
	void AbortAcquisition(); // Abort any running acquisition
	void InputTimerTick(); // Input FPS limiter timer tick
	bool GetFrameData(void** data, uns32* frameBytes, pm::Frame::Info frameInfo);

private: // IFpsLimiterListener
    virtual void OnFpsLimiterEvent(pm::FpsLimiter* sender,
            std::shared_ptr<pm::Frame> frame) override;

private:
	bool InitAcquisition();
	void UninitAcquisition();

	bool acq_ready = false;
	bool acq_active = false;

	pm::Settings m_settings;
	pm::OptionController m_optionController;
	unsigned int m_targetFps; // Not stored in Settings
	std::shared_ptr<pm::Camera> m_camera;
	std::shared_ptr<pm::Acquisition> m_acquisition;
	std::shared_ptr<pm::FpsLimiter> m_fpslimiter;
	std::shared_ptr<pm::Frame> m_frame;
};