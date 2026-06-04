/*
 * File:   second_chance.hpp
 * Second Chance (Clock) page replacement algorithm.
 *
 * Pages are arranged in a circular buffer. Each page has a reference bit (R).
 * A clock hand circulates through the buffer.
 *
 * On page fault:
 *   - Check the page at the clock hand position.
 *   - If R=0: evict this page, insert new page with R=1, advance hand.
 *   - If R=1: clear R to 0, advance hand, repeat (give a "second chance").
 *
 * On hit: set the page's R bit to 1.
 *
 * This APPROXIMATES LRU — frequently used pages keep their R bits set.
 */

#ifndef _SECOND_CHANCE_HPP_INCLUDED_
#define _SECOND_CHANCE_HPP_INCLUDED_

#include <vector>
#include <string>
#include <cstddef>

namespace page {

class SecondChance {
public:
    SecondChance(int num_frames)
        : _num_frames(num_frames), _clock_hand(0), _page_faults(0)
    {
        _frames.reserve(num_frames);
    }

    // Returns true if a page fault occurred
    bool reference(int page) {
        // Check if page is already in a frame (hit)
        for (size_t i = 0; i < _frames.size(); ++i) {
            if (_frames[i].page == page) {
                _frames[i].ref_bit = 1;  // give it a second chance
                return false;
            }
        }

        // Page fault
        _page_faults++;

        if ((int)_frames.size() < _num_frames) {
            // Free frame available — insert at current hand position
            _frames.push_back({page, 1});
            // Note: for simplicity, we add to end. Hand stays.
            // But we should insert at hand position for proper clock behavior.
        } else {
            // No free frame: use clock algorithm to find a victim
            while (true) {
                if (_frames[_clock_hand].ref_bit == 0) {
                    // Found victim: replace this page
                    _frames[_clock_hand].page = page;
                    _frames[_clock_hand].ref_bit = 1;
                    // Advance hand and break
                    _clock_hand = (_clock_hand + 1) % _num_frames;
                    break;
                } else {
                    // Give second chance: clear bit, move on
                    _frames[_clock_hand].ref_bit = 0;
                    _clock_hand = (_clock_hand + 1) % _num_frames;
                }
            }
        }
        return true;
    }

    size_t getPageFaults() const { return _page_faults; }

    void reset() {
        _frames.clear();
        _clock_hand = 0;
        _page_faults = 0;
    }

    const std::string name() const { return "Second Chance"; }

private:
    struct Frame {
        int page;
        int ref_bit;  // 0 or 1
    };

    std::vector<Frame> _frames;
    int    _num_frames;
    int    _clock_hand;
    size_t _page_faults;
};

} // namespace page

#endif /* _SECOND_CHANCE_HPP_INCLUDED_ */
