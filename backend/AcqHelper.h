/* System */
#include <string>

/* Local */
#include "Acquisition.h"
#include "OptionController.h"
#include "Camera.h"
#include "Settings.h"

class Helper
{
public:
	Helper();
	~Helper();

	bool aborted = false;

	bool InstallTerminationHandlers();
	bool ApplySettings(uns32 expTotal, uns32 expTime, int16 expMode, const std::vector<rgn_type>& regions, const char *path);
	void ShowSettings();
	bool AttachCamera(std::string camName); // Attach/connect camera
	bool RunAcquisition(); // Run the acquisition
	static void AbortAcquisition(); // Abort any running acquisition

private: // CLI option handlers
	bool HandleTargetFps(const std::string& value);

	bool InitAcquisition();
	void UninitAcquisition();

	bool acq_ready = false;
	pm::Settings m_settings;
	pm::OptionController m_optionController;
	unsigned int m_targetFps; // Not stored in Settings
	std::shared_ptr<pm::Camera> m_camera;
	std::shared_ptr<pm::Acquisition> m_acquisition;
};