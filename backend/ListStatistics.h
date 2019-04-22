#pragma once
#ifndef PM_LIST_STATISTICS_H
#define PM_LIST_STATISTICS_H

/* System */
#include <set>
#include <limits>

namespace pm {

// Stores sorted and unique items in the list.
template <typename T>
class ListStatistics
{
public:
    ListStatistics()
    {}

    ~ListStatistics()
    {}

    /* Removes all items added so far. */
    void Clear()
    { m_data.clear(); }

    /* Add new item to the list.
       Returns false if the same item is already in the list. */
    bool AddItem(T item)
    {
        return m_data.insert(item).second;
    }

    // Returns number of items in list
    size_t GetCount() const
    { return m_data.size(); }

    /* This algorithm requires slight explanation:
       This algorithm is intended to find the average difference between two
       consecutively-valued elements of the list (requires sorting). */
    double GetAvgSpacing() const
    {
        /* If there's only 1 item in the list the average value-distance must be 0 */
         if (GetCount() <= 1)
            return 0;

        /* Go through items 1-at-a-time and accumulate the distance in terms of
           their values, and then divide by the number of items in the list-1 */
        typename std::set<T>::const_iterator m_dataItPrev = m_data.begin();
        typename std::set<T>::const_iterator m_dataIt = m_data.begin();
        ++m_dataIt;

        double result = 0;

        /* Start at the second element because we need pairs to do this measurement */
        for (; m_dataIt != m_data.end(); ++m_dataIt, ++m_dataItPrev)
            result += (*m_dataIt - *m_dataItPrev);

        result /= GetCount() - 1;

        return result;
    }

    /* This algorithm calculates the largest group of consecutively-valued
       elements in the list. */
    size_t GetLargestCluster() const
    {

        if (GetCount() == 0)
            return 0;

        if (GetCount() == 1)
            return 1;

        /* Flag that indicates if we found > 1 set of consecutively-valued elements */
        size_t result = 1;
        size_t tempResult = 1;

        typename std::set<T>::const_iterator m_dataItPrev = m_data.begin();
        typename std::set<T>::const_iterator m_dataIt = m_data.begin();
        ++m_dataIt;

        for (; m_dataIt != m_data.end(); ++m_dataIt, ++m_dataItPrev)
        {
            if (*m_dataIt == (*m_dataItPrev) + 1)
            {
                tempResult++;

                /* If this cluster is bigger than we've ever seen before, record it */
                if (tempResult > result)
                    result = tempResult;
            }
            else
            {
                tempResult = 1;
            }
        }

        return result;
    }

private:
    std::set<T> m_data;
};

} // namespace pm

#endif /* PM_LIST_STATISTICS_H */
