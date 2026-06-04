/*
 * File:   lru_stack.hpp
 * LRU page replacement algorithm using stack implementation.
 *
 * A doubly-linked list maintains pages in access order:
 *   - Front (head) = most recently used
 *   - Back (tail)  = least recently used
 *
 * On access: the page is moved to the front of the stack.
 * On eviction: the page at the back of the stack is removed.
 * An unordered_set is used for O(1) hit detection.
 * This is an EXACT LRU implementation.
 */

#ifndef _LRU_STACK_HPP_INCLUDED_
#define _LRU_STACK_HPP_INCLUDED_

#include <list>
#include <unordered_set>
#include <string>
#include <cstddef>
#include <algorithm>

namespace page {

class LruStack {
public:
    LruStack(int num_frames)
        : _num_frames(num_frames), _page_faults(0)
    {}

    // Returns true if a page fault occurred
    bool reference(int page) {
        auto it = _page_set.find(page);

        if (it != _page_set.end()) {
            // Hit: move page to the front of the stack (most recently used)
            // Find in list and move to front
            auto list_it = std::find(_stack.begin(), _stack.end(), page);
            _stack.erase(list_it);
            _stack.push_front(page);
            return false;
        }

        // Page fault
        _page_faults++;

        if ((int)_stack.size() < _num_frames) {
            // Still have free frames
            _stack.push_front(page);
            _page_set.insert(page);
        } else {
            // Need to evict: remove the back (LRU) page
            int evicted = _stack.back();
            _stack.pop_back();
            _page_set.erase(evicted);

            // Insert new page at front
            _stack.push_front(page);
            _page_set.insert(page);
        }
        return true;
    }

    size_t getPageFaults() const { return _page_faults; }

    void reset() {
        _stack.clear();
        _page_set.clear();
        _page_faults = 0;
    }

    const std::string name() const { return "LRU Stack"; }

private:
    std::list<int> _stack;              // front=MRU, back=LRU
    std::unordered_set<int> _page_set;  // O(1) hit detection
    int _num_frames;
    size_t _page_faults;
};

} // namespace page

#endif /* _LRU_STACK_HPP_INCLUDED_ */
