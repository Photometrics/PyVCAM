/* System */
#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <string>
#include <thread>

#if defined(_WIN32)
	#include <Windows.h>
#else
	#include <cstring>
	#include <signal.h>
#endif

/* PVCAM */
#include <master.h>
#include <pvcam.h>

/* Local */
#include "Acquisition.h"
#include "Camera.h"
#include "ConsoleLogger.h"
#include "Frame.h"
#include "Log.h"
#include "PrdFileFormat.h"
#include "RealCamera.h"
#include "Settings.h"
#include "TrackRuntimeLoader.h"
#include "Utils.h"
#include "AcqHelper.h"

// Global variables, used only for termination handlers!
// Global copy of Acquisition pointer
std::shared_ptr<pm::Acquisition> g_acquisition(nullptr);
// Global flag to abort current operation
std::atomic<bool> g_userAbortFlag(false);

using HandlerProto = bool(Helper::*)(const std::string&);

Helper::Helper()
	: m_settings(),
	m_optionController(),
	m_camera(nullptr),
	m_acquisition(nullptr)
{
}

Helper::~Helper()
{
	UninitAcquisition();
}

bool Helper::HandleTargetFps(const std::string& value)
{
	return pm::StrToNumber<unsigned int>(value, m_targetFps);
}

bool Helper::ApplySettings(uns16 camIndex, uns32 expTotal, uns32 expTime, int16 expMode, const std::vector<rgn_type>& regions, const char *path)
{
	m_settings.SetCamIndex(camIndex);
	m_settings.SetAcqFrameCount(expTotal);
	m_settings.SetExposure(expTime);
	m_settings.SetRegions(regions);
	// MAKE THIS CHANGEABLE
	m_settings.SetAcqMode(pm::AcqMode::SnapCircBuffer);
	m_settings.SetStorageType(pm::StorageType::Tiff);
	m_settings.SetMaxStackSize(2147483647); // 0xFFFFFFFF / 2 bytes (~2.15 GB)
	m_settings.SetSaveDir(path);
	return true;
}

bool Helper::InitAcquisition()
{
	// Get Camera instance
	m_camera = std::make_shared<pm::RealCamera>();
	if (!m_camera)
	{
		pm::Log::LogE("Failure getting Camera instance!!!");
		return false;
	}

	if (!m_camera->Initialize())
		return false;

	// Get Acquisition instance
	m_acquisition = std::make_shared<pm::Acquisition>(m_camera);
	if (!m_acquisition)
	{
		pm::Log::LogE("Failure getting Acquisition instance!!!");
		return false;
	}

	return true;
}

void Helper::UninitAcquisition()
{
	if (m_acquisition)
	{
		// Ignore errors
		m_acquisition->RequestAbort();
		m_acquisition->WaitForStop(false);
	}

	if (m_camera)
	{
		// Ignore errors
		if (m_camera->IsOpen())
		{
			if (!m_camera->Close())
			{
				pm::Log::LogE("Failure closing camera");
			}
		}
		if (!m_camera->Uninitialize())
		{
			pm::Log::LogE("Failure uninitializing PVCAM");
		}
	}

	m_acquisition = nullptr;
	m_camera = nullptr;
}

bool Helper::RunAcquisition()
{
	if (!InitAcquisition())
	{
		return false;
	}

	int16 totalCams;
	if (!m_camera->GetCameraCount(totalCams))
	{
		totalCams = 0;
	}
	pm::Log::LogI("We have %d camera(s)", totalCams);

	const int16 camIndex = m_settings.GetCamIndex();
	if (camIndex >= totalCams)
	{
		pm::Log::LogE("There is not enough cameras to select index %d",
			camIndex);
		return false;
	}

	std::string camName;
	if (!m_camera->GetName(camIndex, camName))
		return false;

	if (!m_camera->Open(camName))
		return false;

	if (!m_camera->ReviseSettings(m_settings, m_optionController, false))
		return false;

	// With no region specified use full sensor size
	if (m_settings.GetRegions().empty())
	{
		std::vector<rgn_type> regions;
		rgn_type rgn;
		rgn.s1 = 0;
		rgn.s2 = m_settings.GetWidth() - 1;
		rgn.sbin = m_settings.GetBinningSerial();
		rgn.p1 = 0;
		rgn.p2 = m_settings.GetHeight() - 1;
		rgn.pbin = m_settings.GetBinningParallel();
		regions.push_back(rgn);
		// Cannot fail, the only region uses correct binning factors
		m_settings.SetRegions(regions);
	}

	// Setup acquisition with m_settings
	if (!m_camera->SetupExp(m_settings))
	{
		pm::Log::LogE("Please review your settings "
			"and ensure they are supported by this camera");
		return false;
	}

	// Run acquisition
	g_userAbortFlag = false;
	if (m_acquisition->Start())
	{
		g_acquisition = m_acquisition;
		m_acquisition->WaitForStop(true);
		g_acquisition = nullptr;
	}

	return true;
}

/*

*******************************************************
* Abort handling. Abort an acquisition using <CTRL>+C *
*******************************************************
*/

#if defined(_WIN32)
static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
	/* Return TRUE if handled this message, further handler functions won't be called.
	Return FALSE to pass this message to further handlers until default handler
	calls ExitProcess(). */

	switch (dwCtrlType)
	{
	case CTRL_C_EVENT: // Ctrl+C
	case CTRL_BREAK_EVENT: // Ctrl+Break
	case CTRL_CLOSE_EVENT: // Closing the console window
	case CTRL_LOGOFF_EVENT: // User logs off. Passed only to services!
	case CTRL_SHUTDOWN_EVENT: // System is shutting down. Passed only to services!
		break;
	default:
		pm::Log::LogE("Unknown console control type!");
		return FALSE;
	}

	if (g_acquisition)
	{
		// On first abort it gives a chance to finish processing.
		// On second abort it forces full stop.
		g_acquisition->RequestAbort(g_userAbortFlag);
		pm::Log::LogI((!g_userAbortFlag)
			? "\n>>> Acquisition stop requested\n"
			: "\n>>> Acquisition interruption forced\n");
		g_userAbortFlag = true;
	}

	return TRUE;
}
#else
static void TerminalSignalHandler(int sigNum)
{
	if (g_acquisition)
	{
		// On first abort it gives a chance to finish processing.
		// On second abort it forces full stop.
		g_acquisition->RequestAbort(g_userAbortFlag);
		pm::Log::LogI((!g_userAbortFlag)
			? "\n>>> Acquisition stop requested\n"
			: "\n>>> Acquisition interruption forced\n");
		g_userAbortFlag = true;
	}
}
#endif

// Sets handlers that properly end acquisition on Ctrl+C, Ctrl+Break, Log-off, etc.
bool Helper::InstallTerminationHandlers()
{
	bool retVal;

#if defined(_WIN32)
	retVal = (TRUE == SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE));
#else
	struct sigaction newAction;
	memset(&newAction, 0, sizeof(newAction));
	newAction.sa_handler = TerminalSignalHandler;
	retVal = true;
	if (0 != sigaction(SIGINT, &newAction, NULL)
		|| 0 != sigaction(SIGHUP, &newAction, NULL)
		|| 0 != sigaction(SIGTERM, &newAction, NULL))
		retVal = false;
#endif

	if (!retVal)
		pm::Log::LogE("Unable to install termination handler(s)!");

	return retVal;
}