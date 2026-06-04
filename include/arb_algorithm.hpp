/*
 * File:   arb_algorithm.hpp
 * Additional-Reference-Bits (ARB) page replacement algorithm.
 *
 * Each frame maintains an 8-bit reference byte that encodes the page's
 * usage history. At periodic intervals (timer ticks), the current reference
 * bit is shifted into the high-order bit of the reference byte.
 *
 * On page fault, the page with the smallest reference byte value is evicted.
 * This APPROXIMATES LRU — higher bit values mean more recent use.
 *
 * Timer interval default: every 64 references.
 */

#ifndef _ARB_ALGORITHM_HPP_INCLUDED_
#define _ARB_ALGORITHM_HPP_INCLUDED_

#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>
#include <climits>
#include <algorithm>

namespace page {

class ArbAlgorithm {
public:
    ArbAlgorithm(int num_frames, int timer_interval = 64)
        : _num_frames(num_frames),
          _timer_interval(timer_interval),
          _ref_counter(0),
          _page_faults(0)
    {
        _frames.reserve(num_frames);
    }

    // Returns true if a page fault occurred
    bool reference(int page) {
        // Periodic timer: shift reference bits
        _ref_counter++;
        if (_ref_counter >= _timer_interval) {
            _ref_counter = 0;
            timerInterrupt();
        }

        // Check if page is already in a frame (hit)
        for (size_t i = 0; i < _frames.size(); ++i) {
            if (_frames[i].page == page) {
                _frames[i].ref_bit = 1;  // hardware sets reference bit
                return false;
            }
        }

        // Page fault
        _page_faults++;

        if ((int)_frames.size() < _num_frames) {
            // Free frame available
            Frame f = {page, 0, 1};
            _frames.push_back(f);  // ref_byte=0, ref_bit=1
        } else {
            // Need to evict: find smallest reference byte
            size_t evict_index = 0;
            uint8_t min_val = _frames[0].ref_byte;
            for (size_t i = 1; i < _frames.size(); ++i) {
                if (_frames[i].ref_byte < min_val) {
                    min_val = _frames[i].ref_byte;
                    evict_index = i;
                }
            }
            // Replace evicted page
            Frame f2 = {page, 0, 1};
            _frames[evict_index] = f2;
        }
        return true;
    }

    size_t getPageFaults() const { return _page_faults; }

    void reset() {
        _frames.clear();
        _ref_counter = 0;
        _page_faults = 0;
    }

    const std::string name() const { return "ARB"; }

private:
    struct Frame {
        int     page;
        uint8_t ref_byte;  // 8-bit reference history
        int     ref_bit;   // current reference bit (0 or 1)
    };

    // Timer interrupt: shift ref_bit into ref_byte for all frames
    void timerInterrupt() {
        for (size_t i = 0; i < _frames.size(); ++i) {
            // Shift right, put ref_bit into highest bit (bit 7)
            _frames[i].ref_byte = (_frames[i].ref_byte >> 1)
                                | (_frames[i].ref_bit << 7);
            _frames[i].ref_bit = 0;  // clear hardware reference bit
        }
    }

    std::vector<Frame> _frames;
    int    _num_frames;
    int    _timer_interval;
    int    _ref_counter;
    size_t _page_faults;
};

} // namespace page

#endif /* _ARB_ALGORITHM_HPP_INCLUDED_ */
