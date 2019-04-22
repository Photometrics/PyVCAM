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

public:
	void ShowHelp();
	bool ApplySettings(uns16 camIndex, uns32 expTotal, uns32 expTime, int16 expMode, const std::vector<rgn_type>& regions, const char *path);
	bool RunAcquisition();

private: // CLI option handlers
	bool HandleTargetFps(const std::string& value);

private:
	bool InitAcquisition();
	void UninitAcquisition();

private:
	pm::Settings m_settings;
	pm::OptionController m_optionController;
	unsigned int m_targetFps; // Not stored in Settings
	std::shared_ptr<pm::Camera> m_camera;
	std::shared_ptr<pm::Acquisition> m_acquisition;
};