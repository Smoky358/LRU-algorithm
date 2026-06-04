/*
 * File:   lru_counter.hpp
 * LRU page replacement algorithm using counter (timestamp) implementation.
 *
 * Each frame stores a page and its last-access timestamp.
 * A global clock increments on every reference() call.
 * On page fault, the page with the smallest timestamp (least recently used) is evicted.
 * This is an EXACT LRU implementation with O(n) eviction.
 */

#ifndef _LRU_COUNTER_HPP_INCLUDED_
#define _LRU_COUNTER_HPP_INCLUDED_

#include <vector>
#include <string>
#include <cstddef>
#include <climits>
#include <algorithm>

namespace page {

class LruCounter {
public:
    LruCounter(int num_frames)
        : _num_frames(num_frames), _clock(0), _page_faults(0)
    {
        _frames.reserve(num_frames);
    }

    // Returns true if a page fault occurred
    bool reference(int page) {
        _clock++;

        // Check if page is already in a frame (hit)
        for (size_t i = 0; i < _frames.size(); ++i) {
            if (_frames[i].page == page) {
                _frames[i].timestamp = _clock;
                return false; // hit, no page fault
            }
        }

        // Page fault — page not in memory
        _page_faults++;

        if ((int)_frames.size() < _num_frames) {
            // Still have free frames, just add
            _frames.push_back({page, _clock});
        } else {
            // Need to evict: find the page with smallest timestamp (LRU)
            size_t lru_index = 0;
            size_t min_ts = _frames[0].timestamp;
            for (size_t i = 1; i < _frames.size(); ++i) {
                if (_frames[i].timestamp < min_ts) {
                    min_ts = _frames[i].timestamp;
                    lru_index = i;
                }
            }
            _frames[lru_index] = {page, _clock};
        }
        return true;
    }

    size_t getPageFaults() const { return _page_faults; }

    void reset() {
        _frames.clear();
        _clock = 0;
        _page_faults = 0;
    }

    const std::string name() const { return "LRU Counter"; }

private:
    struct Frame {
        int page;
        size_t timestamp;
    };

    std::vector<Frame> _frames;
    int _num_frames;
    size_t _clock;
    size_t _page_faults;
};

} // namespace page

#endif /* _LRU_COUNTER_HPP_INCLUDED_ */
