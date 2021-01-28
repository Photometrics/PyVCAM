#include <iostream>
#include <limits>

#include "gtest/gtest.h"

#include "Acquisition.h"
#include "FakeCamera.h"
#include "Consolelogger.h"
#include "Fpslimiter.h"


TEST(acquisition_test_case, acquisition)
{
    // Uncomment the following line to print log messages to the console
	// auto consoleLogger = std::make_shared<pm::ConsoleLogger>();

	pm::Settings settings;

    unsigned int target_fps = UINT_MAX; // fake camera rapid-fires frames
    std::shared_ptr<pm::Camera> camera = std::make_shared<pm::FakeCamera>(target_fps);

	ASSERT_TRUE(camera->Initialize());
	ASSERT_TRUE(camera->Open("FakeCamera"));

    std::shared_ptr<pm::Acquisition> acquisition =
        std::make_shared<pm::Acquisition>(camera);

    std::vector<rgn_type> regions;
    rgn_type rgn = {0, 2960, 1, 1380, 1579, 1}; // s1, s2, sbin, p1, p2, pbin
    regions.push_back(rgn);

    // Apply settings
	settings.SetRegions(regions);
	settings.SetAcqFrameCount(5000);
	settings.SetExposure(1);
	settings.SetAcqMode(pm::AcqMode::SnapCircBuffer);
	settings.SetStorageType(pm::StorageType::None);
	settings.SetMaxStackSize(2000000000); // (2 GB)

	ASSERT_TRUE(camera->SetupExp(settings));
	ASSERT_TRUE(acquisition->Start(nullptr));

	bool aborted = acquisition->WaitForStop(true);
    ASSERT_FALSE(aborted);

    // Make sure all frames were processed by the acquisition thread
    double fps = 0.1;
    size_t valid_frames = 99;
    size_t lost_frames = 99;
    size_t max_frames = 99;
    size_t cached_frames = 99;
    acquisition->GetAcqStats(fps, valid_frames, lost_frames, max_frames, cached_frames);
    ASSERT_EQ(valid_frames, 5000);
    ASSERT_EQ(lost_frames, 0);

    // Make sure all frames were processed by the "save to disk" thread
    valid_frames = 99;
    lost_frames = 99;
    acquisition->GetDiskStats(fps, valid_frames, lost_frames, max_frames, cached_frames);
    ASSERT_EQ(valid_frames, 5000);
    ASSERT_EQ(lost_frames, 0);
}
