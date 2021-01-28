#include "gtest/gtest.h"

#include "LostFrameTracker.h"


TEST(lost_frame_tracker, nominal_case)
{
    pm::LostFrameTracker tracker;

    ASSERT_EQ(tracker.GetCount(), 0);
    ASSERT_EQ(tracker.GetLargestCluster(), 0);
    ASSERT_EQ(tracker.GetAvgSpacing(), 0.0);

    tracker.AddRange(11, 15);

    ASSERT_EQ(tracker.GetCount(), 5);
    ASSERT_EQ(tracker.GetLargestCluster(), 5);
    ASSERT_NEAR(tracker.GetAvgSpacing(), 1.0, 1.0e-6);

    tracker.AddItem(40);
    tracker.AddRange(21, 30);

    ASSERT_EQ(tracker.GetCount(), 16);
    ASSERT_EQ(tracker.GetLargestCluster(), 10);
    ASSERT_NEAR(tracker.GetAvgSpacing(), 29.0/15.0, 1.0e-6);

    tracker.Clear();
    ASSERT_EQ(tracker.GetCount(), 0);
    ASSERT_EQ(tracker.GetLargestCluster(), 0);
    ASSERT_EQ(tracker.GetAvgSpacing(), 0.0);

    tracker.AddItem(15);
    ASSERT_EQ(tracker.GetCount(), 1);
    ASSERT_EQ(tracker.GetLargestCluster(), 1);
    ASSERT_EQ(tracker.GetAvgSpacing(), 0.0);

    tracker.AddItem(11);

    ASSERT_EQ(tracker.GetCount(), 2);
    ASSERT_EQ(tracker.GetLargestCluster(), 1);
    ASSERT_NEAR(tracker.GetAvgSpacing(), 4.0, 1.0e-6);
}

TEST(lost_frame_tracker, overlapping_ranges)
{
    pm::LostFrameTracker tracker;

    tracker.AddRange(11, 14);
    tracker.AddRange(12, 15);
    tracker.AddRange(21, 26);
    tracker.AddRange(27, 30);

    ASSERT_EQ(tracker.GetCount(), 15);
    ASSERT_EQ(tracker.GetLargestCluster(), 10);
    ASSERT_NEAR(tracker.GetAvgSpacing(), 19.0 / 14.0, 1.0e-6);
}
