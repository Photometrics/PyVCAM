/* System */
#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <string>
#include <thread>

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
	//m_settings.SetMaxStackSize(4000000000); // 4 gigabytes
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
		pm::Log::LogE("There is not so many cameras to select from index %d",
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

	/* One additional note, is that the print statements in this code
	are for demonstration only, and it is not normally recommended
	to print this verbosely during an acquisition, because it may
	affect the performance of the system. */
	if (!m_camera->SetupExp(m_settings))
	{
		pm::Log::LogE("Please review your commandline parameters "
			"and ensure they are supported by this camera");
		return false;
	}

	if (m_acquisition->Start())
	{
		m_acquisition->WaitForStop(true);
	}

	return true;
}