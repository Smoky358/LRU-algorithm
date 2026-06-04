cpp-lru-cache
=============

Simple and reliable LRU (Least Recently Used) cache for c++ based on hashmap and linkedlist. The library is header only, simple test and example are included.
It includes standard components and very little own logics that guarantees reliability.

Example:
--------

### LRU Cache (header-only)

```
/** Creates cache with maximum size of three. When the
    size is achieved every next element will replace the
    least recently used one. */
cache::lru_cache<std::string, std::string> cache(3);

cache.put("one", "one");
cache.put("two", "two");

const std::string& from_cache = cache.get("two");
```

### Page Replacement Simulator

Simulates four page replacement algorithms (LRU Counter, LRU Stack,
Additional-Reference-Bits, Second Chance) on various memory access traces
and outputs results as tables and ASCII bar charts.

```
cd build
make page_simulator
./page_simulator
```

Optional parameters:

```
./page_simulator --frames 4,8,16       # frame counts to test
./page_simulator --pages 5000           # page references per trace
./page_simulator --trace random         # specific trace (sequential, hotloop, random, locality, mixed, all)
```

How to run tests:

### Original LRU Cache Tests

```
mkdir build
cd build
cmake ..
make check
```

### Page Replacement Simulator

```
cd build
make page_simulator
./page_simulator
```

To customize the simulation:

```
./page_simulator --frames 2,4,8,16,32,64    # frame counts (default)
./page_simulator --pages 10000               # references per trace (default)
./page_simulator --space 200                 # address space size (default)
./page_simulator --trace all                 # trace: sequential, hotloop, random, locality, mixed, all
./page_simulator --help                      # show all options
```
