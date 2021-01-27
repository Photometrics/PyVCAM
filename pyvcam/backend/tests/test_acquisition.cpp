#include <iostream>

#include "gtest/gtest.h"

#include "Acquisition.h"
#include "FakeCamera.h"
#include "Consolelogger.h"
#include "Fpslimiter.h"


auto noop_fpslimiter_callback(std::shared_ptr<pm::Frame> frame)
{
};


TEST(test_test_case, sample_test)
{
    EXPECT_EQ(1, 1);
}

TEST(acquisition_test_case, acquisition)
{
    // Start log
	auto consoleLogger = std::make_shared<pm::ConsoleLogger>();

	pm::Settings settings;

    std::shared_ptr<pm::Camera> camera =
        std::make_shared<pm::FakeCamera>(1000000000);

	ASSERT_TRUE(camera->Initialize());
	ASSERT_TRUE(camera->Open("FakeCamera"));

    std::shared_ptr<pm::Acquisition> acquisition =
        std::make_shared<pm::Acquisition>(camera);

    std::vector<rgn_type> regions;
    rgn_type rgn;
    rgn.s1 = 0;
    rgn.s2 = 2960;
    rgn.sbin = 1;
    rgn.p1 = 1380;
    rgn.p2 = 1579;
    rgn.pbin = 1;
    regions.push_back(rgn);

    // Apply settings
	settings.SetRegions(regions);
	settings.SetAcqFrameCount(1000);
	settings.SetExposure(10);
	settings.SetAcqMode(pm::AcqMode::SnapCircBuffer);
	settings.SetStorageType(pm::StorageType::None);
	settings.SetMaxStackSize(2000000000); // (2 GB)

	ASSERT_TRUE(camera->SetupExp(settings));
	ASSERT_TRUE(acquisition->Start(nullptr));

	bool aborted = acquisition->WaitForStop(true);
    ASSERT_FALSE(aborted);
}

// TEST(acquisition_test_case, live)
// {
//     // Start log
// 	auto consoleLogger = std::make_shared<pm::ConsoleLogger>();
//
// 	pm::Settings settings;
//
//     std::shared_ptr<pm::Camera> camera =
//         std::make_shared<pm::FakeCamera>(100);
//
// 	ASSERT_TRUE(camera->Initialize());
// 	ASSERT_TRUE(camera->Open("FakeCamera"));
//
//     std::shared_ptr<pm::Acquisition> acquisition =
//         std::make_shared<pm::Acquisition>(camera);
// 	std::shared_ptr<pm::FpsLimiter> fpslimiter =
//         std::make_shared<pm::FpsLimiter>();
//
//     ASSERT_TRUE(fpslimiter->Start(noop_fpslimiter_callback));
//
//     std::vector<rgn_type> regions;
//     rgn_type rgn;
//     rgn.s1 = 0;
//     rgn.s2 = 2960;
//     rgn.sbin = 1;
//     rgn.p1 = 0;
//     rgn.p2 = 2960;
//     rgn.pbin = 1;
//     regions.push_back(rgn);
//
//     // Apply settings
// 	settings.SetRegions(regions);
// 	settings.SetAcqFrameCount(1000);
// 	settings.SetExposure(10);
// 	settings.SetAcqMode(pm::AcqMode::LiveCircBuffer);
// 	settings.SetStorageType(pm::StorageType::None);
// 	settings.SetMaxStackSize(2000000000); // (2 GB)
//
// 	ASSERT_TRUE(camera->SetupExp(settings));
// 	ASSERT_TRUE(acquisition->Start(fpslimiter));
//
// 	bool aborted = acquisition->WaitForStop(true);
//     ASSERT_FALSE(aborted);
// }
