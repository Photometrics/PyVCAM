// Home of the LostFrameTracker class
// Tracks lost frames

#pragma once
#ifndef PM_LOST_FRAME_TRACKER_H
#define PM_LOST_FRAME_TRACKER_H

/* System */
#include <deque>
#include <limits>

namespace pm {

// Calculates statistics for lost frame numbers.
class LostFrameTracker
{
    using LostFramesRange = std::pair<uint32_t, uint32_t>;

public:
    // default constructors and destructor are fine

    // Removes all items added so far
    void Clear();

    // Add new lost frame number
    void AddItem(uint32_t lost_frame_number);

    // Add range of lost frame numbers
    // (the range is inclusive)
    void AddRange(uint32_t first_frame_number, uint32_t last_frame_number);

    // Returns total number of lost frames
    size_t GetCount() const;

    // Returns the average difference between two
    // consecutively-valued lost frames
    double GetAvgSpacing() const;

    // Returns the largest group of consecutively-valued lost frames
    size_t GetLargestCluster() const;

private:
    void SortAndCollapse();
    static bool CompareLostFrameRanges(LostFramesRange lhs, LostFramesRange rhs);

private:
    // deque chosen for performance of insertions at end
    std::deque<LostFramesRange> m_ranges;
    bool m_sorted_collapsed = false;
};

} // namespace pm

#endif /* PM_LOST_FRAME_TRACKER_H */
