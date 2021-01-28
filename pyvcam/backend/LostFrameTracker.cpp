// Copyright (C) Spatial Genomics, Inc.
// All rights reserved.
//
// Home of the LostFrameTracker class
// Tracks lost frames

#include <algorithm>

#include "LostFrameTracker.h"


// Removes all items added so far
void pm::LostFrameTracker::Clear()
{
    m_ranges.clear();
    m_sorted_collapsed = false;
}

// Add new lost frame number
void pm::LostFrameTracker::AddItem(uint32_t lost_frame_number)
{
    AddRange(lost_frame_number, lost_frame_number);
}

// Add range of lost frame numbers
// (the range is inclusive)
void pm::LostFrameTracker::AddRange(uint32_t first_frame_number, uint32_t last_frame_number)
{
    if (first_frame_number > last_frame_number)
        return;

    m_ranges.push_back(std::make_pair(first_frame_number, last_frame_number));
    m_sorted_collapsed = false;
}

bool pm::LostFrameTracker::CompareLostFrameRanges(
    LostFramesRange lhs,
    LostFramesRange rhs)
{
    if (lhs.first < rhs.first)
        return true;
    if (lhs.first > rhs.first)
        return false;

    return lhs.second < rhs.second;
}

void pm::LostFrameTracker::SortAndCollapse()
{
    if (m_sorted_collapsed)
        return;

    if (m_ranges.empty())
        return;

    // sort
    std::sort(m_ranges.begin(), m_ranges.end(), CompareLostFrameRanges);

    // collapse
    // if there is no gap between consecutuve ranges then combine them
    std::deque<LostFramesRange> new_ranges;
    auto prior = m_ranges.begin();
    for (auto iterator = prior + 1; iterator != m_ranges.end(); iterator++)
    {
        if (iterator->first > prior->second + 1)
        {
            // got distinct ranges of lost frames
            new_ranges.push_back(*prior);
            prior = iterator;
        }
        else
        {
            // no gap between the 2 ranges of lost frames, combine them
            if (prior->second < iterator->second)
                prior->second = iterator->second;
        }
    }

    new_ranges.push_back(*prior);
    m_ranges = new_ranges;
    m_sorted_collapsed = true;
}

// Returns total number of lost frames
size_t pm::LostFrameTracker::GetCount() const
{
    const_cast<pm::LostFrameTracker*>(this)->SortAndCollapse();

    size_t lost_frame_count = 0;
    for (auto iterator = m_ranges.cbegin(); iterator != m_ranges.cend(); iterator++)
    {
        lost_frame_count += 1 + iterator->second - iterator->first;
    }

    return lost_frame_count;
}


// Returns the average difference between two
// consecutively-valued lost frames
double pm::LostFrameTracker::GetAvgSpacing() const
{
    if (m_ranges.empty())
        return 0.0;

    const_cast<pm::LostFrameTracker*>(this)->SortAndCollapse();

    double sum_lengths = 0;
    size_t observations = 0;
    for (auto iterator = m_ranges.cbegin();;)
    {
        if (iterator->second - iterator->first > 0)
        {
            uint32_t range_length = iterator->second - iterator->first;
            sum_lengths += range_length;
            observations += range_length;
        }

        auto prior = iterator;
        if (++iterator == m_ranges.cend())
            break;

        uint32_t gap_length = iterator->first - prior->second;
        sum_lengths += gap_length;
        observations++;
    }

    if (observations < 1)
        return 0.0;

    double average_spacing = sum_lengths / observations;
    return average_spacing;
}


// Returns the length of the largest group of consecutively-valued lost frames
size_t pm::LostFrameTracker::GetLargestCluster() const
{
    const_cast<pm::LostFrameTracker*>(this)->SortAndCollapse();

    size_t longest_len = 0;
    for (auto iterator = m_ranges.cbegin(); iterator != m_ranges.cend(); iterator++)
    {
        uint32_t length = 1 + iterator->second - iterator->first;
        if (longest_len < length)
            longest_len = length;
    }

    return longest_len;
}
