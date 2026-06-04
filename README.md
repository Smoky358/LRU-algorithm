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
Additional-Reference-Bits, Second Chance) on page reference traces
and outputs results as tables and ASCII bar charts.

Three modes are supported:

**Mode 1 — Single trace file** (supports `.gz` and hex‑address traces):
```
cd build
make page_simulator
./page_simulator --file traces/gcc.log
./page_simulator --file traces/emacs.gz --frames 4,8,16,32 --pagesize 4096
```

**Mode 2 — Built-in synthetic traces** (Classic, TightLoop, Program, Locality, Sequential, Random):
```
./page_simulator                        # quick subset
./page_simulator --benchmark            # full benchmark
./page_simulator --benchmark --pages 5000 --space 50
```

**Mode 3 — Batch trace directory** (all `.gz` files in a folder):
```
./page_simulator --tracedir /path/to/traces
./page_simulator --tracedir /path/to/traces --limit 100000  # quick test
```

All options: `--file`, `--tracedir`, `--pagesize`, `--frames`, `--benchmark`, `--pages`, `--space`, `--limit`, `--help`.

How to run tests:

### Original LRU Cache Unit Tests

```
mkdir build
cd build
cmake ..
make check
```

### Page Replacement Simulator

Build the simulator:

```
cd build
make page_simulator
```

#### Mode 1: Single Trace File

Read a page trace from a file (supports `.gz` compressed files and hex‑address traces):

```
./page_simulator --file traces/gcc.log
./page_simulator --file traces/emacs.gz
./page_simulator --file traces/gpp.gz --frames 4,8,16,32 --pagesize 4096
```

#### Mode 2: Built-in Synthetic Traces

Run all built-in traces (Classic1/2, TightLoop, Program, Locality, Sequential, Random):

```
./page_simulator --benchmark
./page_simulator --benchmark --pages 5000 --space 50
```

Without `--benchmark`, a quick subset of built-in traces runs:

```
./page_simulator
./page_simulator --pages 10000 --space 100
```

#### Mode 3: Batch Trace Directory

Run all `.gz` trace files in a directory:

```
./page_simulator --tracedir /path/to/traces
./page_simulator --tracedir /path/to/traces --pagesize 4096 --frames 8,16,32,64,128
./page_simulator --tracedir /path/to/traces --limit 100000    # quick test with limited refs
```

#### All Options

```
--file <path>        Read page trace from file
--tracedir <path>    Read all .gz trace files from directory
--pagesize <n>       Page size in bytes (default: 4096)
--frames <n1,...>    Frame counts (default: auto-scaled)
--benchmark          Run all built-in traces
--pages <n>          References per synthetic trace (default: 1000)
--space <n>          Max page number for synthetic traces (default: 30)
--limit <n>          Limit references per trace (default: unlimited)
--help               Show this help
```

