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
std::shared_ptr<pm::Acquisition> g_acquisition(nullptr);
std::atomic<bool> g_userAbortFlag(false);

using HandlerProto = bool(Helper::*)(const std::string&);

Helper::Helper()
	: m_settings(),
	m_optionController(),
	m_camera(nullptr),
	m_acquisition(nullptr),
	m_fpslimiter(nullptr),
	m_frame(nullptr)
{
}

Helper::~Helper()
{
	UninitAcquisition();
}

bool Helper::ApplySettings(uns32 expTotal, uns32 expTime, int16 expMode, const std::vector<rgn_type>& regions, const char *path)
{
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

void Helper::ShowSettings()
{
	std::cout << "Cam index: " << m_settings.GetCamIndex() << std::endl;
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
	{
		pm::Log::LogE("Failure initializing Camera instance!!!");
		return false;
	}

	// Get FPS Limiter instance
	m_fpslimiter = std::make_shared<pm::FpsLimiter>();
	if (!m_fpslimiter)
	{
		pm::Log::LogE("Failure getting FPS limiter instance!!!");
		return false;
	}

	// Get FPS Listener instance
	if (!m_fpslimiter->Start(this))
	{
		pm::Log::LogE("Failure starting FPS listener instance!!!");
		return false;
	}

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

	if (m_fpslimiter)
	{
		m_fpslimiter->Stop();
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
	m_fpslimiter = nullptr;
	m_camera = nullptr;
	acq_ready = false;
	acq_active = false;
}

bool Helper::AttachCamera(std::string camName)
{
	if (!InitAcquisition())
	{
		return false;
	}

	if (!m_camera->Open(camName))
	{
		return false;
	}

	acq_ready = true;
	return true;
}

bool Helper::StartAcquisition()
{
	if (!acq_ready || acq_active)
		return false;

	// Apply settings
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
	m_userAbortFlag = false;
	if (!m_acquisition->Start(m_fpslimiter))
	{
		g_acquisition = m_acquisition;
		return false;
	}
	else
	{
		acq_active = true;
	}

	return true;
}

bool Helper::JoinAcquisition()
{
	m_acquisition->WaitForStop(true);
	if (g_userAbortFlag) m_userAbortFlag = true;
	g_acquisition = nullptr;
	acq_active = false;

	pm::Log::LogI("Acquisition exited!");
	return true;
}

bool Helper::AcquisitionStatus()
{
	return acq_active;
}

void Helper::AcquisitionStats(double& acqFps, size_t& acqFramesValid,
        size_t& acqFramesLost, size_t& acqFramesMax, size_t& acqFramesCached,
		double& diskFps, size_t& diskFramesValid,
        size_t& diskFramesLost, size_t& diskFramesMax, size_t& diskFramesCached)
{
	m_acquisition->GetAcqStats(acqFps, acqFramesValid,
        acqFramesLost, acqFramesMax, acqFramesCached);
	m_acquisition->GetDiskStats(diskFps, diskFramesValid,
        diskFramesLost, diskFramesMax, diskFramesCached);
}

void Helper::AbortAcquisition()
{
	if (m_acquisition)
	{
		// On first abort it gives a chance to finish processing.
		// On second abort it forces full stop.
		m_acquisition->RequestAbort(m_userAbortFlag);
		pm::Log::LogI((!m_userAbortFlag)
			? "\n>>> Acquisition stop requested\n"
			: "\n>>> Acquisition interruption forced\n");
		m_userAbortFlag = true;
	}
}

void Helper::InputTimerTick()
{
	m_fpslimiter->InputTimerTick();
}

bool Helper::GetFrameData(void** data, uns32* frameBytes, pm::Frame::Info frameInfo)
{
	if (!m_frame || !m_frame->IsValid()) {
		std::cout << "Frame invalid" << std::endl << std::flush;
		return false;
	}
	// Get frame data size
	*frameBytes = (uns32)m_frame->GetAcqCfg().GetFrameBytes();
	frameInfo = m_frame->GetInfo();

	// Allocate mem and copy frame data
	*data = (void*)new uint8_t[*frameBytes];
	if (!*data) {
		std::cout << "Data pointer invalid" << std::endl << std::flush;
		return false;
	}
	memcpy(*data, m_frame->GetData(), *frameBytes);
	return true;
}

void Helper::OnFpsLimiterEvent(pm::FpsLimiter* sender, std::shared_ptr<pm::Frame> frame)
{
	m_frame = frame;
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