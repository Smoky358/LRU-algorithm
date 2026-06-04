/*
 * File:   page_simulator.cpp
 * Page replacement algorithm simulator.
 *
 * Simulates four page replacement algorithms (LRU Counter, LRU Stack,
 * Additional-Reference-Bits, Second Chance) on page reference traces
 * and outputs results as tables and ASCII bar charts.
 *
 * Usage:
 *   ./page_simulator [options]
 *
 * Options:
 *   --file <path>           Read page trace from file (one page number per line)
 *   --frames <n1,n2,...>    Frame counts to test (default: auto-scaled)
 *   --benchmark             Run all built-in traces with all frame sizes
 *   --help                  Show help
 */

#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <set>
#include <dirent.h>
#include <sys/stat.h>

#include "lru_counter.hpp"
#include "lru_stack.hpp"
#include "arb_algorithm.hpp"
#include "second_chance.hpp"

// ============================================================================
// Trace: classic textbook reference strings
// ============================================================================

// Silberschatz "Operating System Concepts" classic example
// Reference string: 7,0,1,2,0,3,0,4,2,3,0,3,2,1,2,0,1,7,0,1
static std::vector<int> getClassicTrace1() {
    int raw[] = {7,0,1,2,0,3,0,4,2,3,0,3,2,1,2,0,1,7,0,1};
    return std::vector<int>(raw, raw + sizeof(raw)/sizeof(raw[0]));
}

// Longer trace simulating program with multiple phases
// Phase 1: sequential init (0..9), Phase 2: tight loop (1,2,3,4,5 repeated),
// Phase 3: scattered access (0,10,20,5,15), Phase 4: sequential re-scan
static std::vector<int> getClassicTrace2() {
    std::vector<int> trace;
    // Init phase: sequential 0..9, repeated twice (program initialization)
    for (int r = 0; r < 2; r++)
        for (int i = 0; i < 10; i++)
            trace.push_back(i);
    // Loop phase: a small loop body
    for (int r = 0; r < 5; r++)
        for (int i = 1; i <= 5; i++)
            trace.push_back(i);
    // Scattered access
    for (int r = 0; r < 3; r++) {
        trace.push_back(0); trace.push_back(10);
        trace.push_back(20); trace.push_back(5);
        trace.push_back(15);
    }
    // Re-scan phase
    for (int r = 0; r < 2; r++)
        for (int i = 0; i < 10; i++)
            trace.push_back(i);
    // Tail loop
    for (int r = 0; r < 5; r++)
        for (int i = 1; i <= 5; i++)
            trace.push_back(i);
    return trace;
}

// ============================================================================
// Trace Generator — models real program access patterns
//
// Real programs exhibit strong temporal & spatial locality:
//   - 90-95% of accesses cluster in a small "hot" working set (3-8 pages)
//   - Instruction fetch is sequential within small ranges (basic blocks)
//   - Stack accesses are highly repetitive
//   - Occasional cold misses to other regions
//
// We model this with varying degrees of locality so users can see
// how algorithms perform from "best case" (tight loop) to "worst case"
// (random / sequential scan).
// ============================================================================

class TraceGenerator {
public:
    TraceGenerator(int num_refs = 1000, int max_page = 30)
        : _num_refs(num_refs), _max_page(max_page)
    {
        std::srand(42);
    }

    // ----------------------------------------------------------------
    // 1. Tight Loop model — STRONGEST locality (like a small inner loop)
    //    95% in 3-4 pages, 5% scattered across address space.
    //    This produces very LOW fault rates with even modest frames.
    // ----------------------------------------------------------------
    std::vector<int> generateTightLoop() {
        std::vector<int> trace;
        trace.reserve(_num_refs);
        int hot_size = 4;  // tiny working set: pages 0..3
        for (int i = 0; i < _num_refs; ++i) {
            if ((std::rand() % 100) < 95)
                trace.push_back(std::rand() % hot_size);             // 95% hot
            else
                trace.push_back(hot_size + (std::rand() % (_max_page - hot_size))); // 5% cold
        }
        return trace;
    }

    // ----------------------------------------------------------------
    // 2. Program model — simulates a real application:
    //    - "Code" pages: sequential walk within small range (spatial locality)
    //    - "Data/stack" pages: heavy reuse of a few pages (temporal locality)
    //    - ~92% within a 5-8 page working set per phase
    // ----------------------------------------------------------------
    std::vector<int> generateProgram() {
        std::vector<int> trace;
        trace.reserve(_num_refs);
        int phases = 6;
        int per_phase = _num_refs / phases;

        for (int p = 0; p < phases; ++p) {
            // Each phase has its own working set in a different region
            int ws_base  = (p * 5) % (_max_page - 8);
            int ws_size  = 5 + (std::rand() % 4);  // 5..8 page working set
            // Code pages: sequential within ws
            int code_start = ws_base;
            int code_end   = ws_base + 3;
            // Data pages: heavily reused
            int data_page  = ws_base + 4;
            int stack_page = ws_base + (ws_size - 1);

            int instr_ptr = code_start;
            for (int i = 0; i < per_phase; ++i) {
                int r = std::rand() % 100;
                if (r < 60) {
                    // 60%: sequential instruction fetch within code pages
                    trace.push_back(instr_ptr);
                    instr_ptr++;
                    if (instr_ptr > code_end) instr_ptr = code_start;
                } else if (r < 85) {
                    // 25%: data/stack access (temporal — same few pages)
                    trace.push_back((r < 73) ? data_page : stack_page);
                } else if (r < 93) {
                    // 8%: other pages within current working set
                    trace.push_back(ws_base + (std::rand() % ws_size));
                } else {
                    // 7%: cold miss — page outside current working set
                    trace.push_back(std::rand() % _max_page);
                }
            }
        }
        return trace;
    }

    // ----------------------------------------------------------------
    // 3. Locality model — 80% hot (10 pages), 20% cold (balance)
    //    Moderate locality — representative of larger applications.
    // ----------------------------------------------------------------
    std::vector<int> generateLocality() {
        std::vector<int> trace;
        trace.reserve(_num_refs);
        int hot_size   = _max_page / 3;        // ~10 pages
        int cold_start = hot_size;
        int cold_size  = _max_page - hot_size;  // ~20 pages
        for (int i = 0; i < _num_refs; ++i) {
            if ((std::rand() % 100) < 80)
                trace.push_back(std::rand() % hot_size);
            else
                trace.push_back(cold_start + (std::rand() % cold_size));
        }
        return trace;
    }

    // ----------------------------------------------------------------
    // 4. Sequential scan — WORST locality (baseline for comparison)
    //    0,1,2,...,M-1, 0,1,2,...  —  every page touched equally.
    // ----------------------------------------------------------------
    std::vector<int> generateSequential() {
        std::vector<int> trace;
        trace.reserve(_num_refs);
        for (int i = 0; i < _num_refs; ++i)
            trace.push_back(i % _max_page);
        return trace;
    }

    // ----------------------------------------------------------------
    // 5. Random uniform — NO locality (baseline for comparison)
    // ----------------------------------------------------------------
    std::vector<int> generateRandom() {
        std::vector<int> trace;
        trace.reserve(_num_refs);
        for (int i = 0; i < _num_refs; ++i)
            trace.push_back(std::rand() % _max_page);
        return trace;
    }

private:
    int _num_refs;
    int _max_page;
};

// ============================================================================
// File-based trace reader
// ============================================================================

// Check if filename ends with .gz
static bool hasGzExtension(const std::string& path) {
    return path.size() >= 3 && path.substr(path.size() - 3) == ".gz";
}

// Check if a line looks like a hex address trace (starts with "0x")
static bool isHexTraceLine(const std::string& line) {
    size_t pos = line.find_first_not_of(" \t\r\n");
    return pos != std::string::npos && pos + 1 < line.size() &&
           line[pos] == '0' && (line[pos+1] == 'x' || line[pos+1] == 'X');
}

// Parse a single hex trace line: "0x<hex> [RW]" -> page number
static bool parseHexTraceLine(const std::string& line, int page_size, int& page) {
    // Extract hex address (first token)
    size_t addr_start = line.find("0x");
    if (addr_start == std::string::npos) {
        addr_start = line.find("0X");
    }
    if (addr_start == std::string::npos) return false;

    char* endp = NULL;
    long addr = std::strtol(line.c_str() + addr_start, &endp, 16);
    if (endp == line.c_str() + addr_start) return false;

    page = (int)(addr / page_size);
    return true;
}

static bool readTraceFile(const std::string& path, std::vector<int>& trace,
                          int page_size = 4096) {
    trace.clear();

    bool is_gz = hasGzExtension(path);

    // Auto-detect: peek at first line to see if it's hex format
    std::string first_line;
    if (is_gz) {
        // Use popen to peek at first line
        std::string cmd = "zcat \"";
        cmd += path;
        cmd += "\" 2>/dev/null";
        FILE* fp = popen(cmd.c_str(), "r");
        if (!fp) {
            std::cerr << "Error: cannot open gzipped trace file: " << path << "\n";
            return false;
        }
        char buf[256];
        if (fgets(buf, sizeof(buf), fp)) {
            first_line = buf;
        }
        pclose(fp);
    } else {
        std::ifstream file(path.c_str());
        if (!file.is_open()) {
            std::cerr << "Error: cannot open trace file: " << path << "\n";
            return false;
        }
        std::getline(file, first_line);
        file.close();
    }

    if (first_line.empty()) {
        std::cerr << "Error: trace file is empty or unreadable: " << path << "\n";
        return false;
    }

    bool is_hex = isHexTraceLine(first_line);

    // Now read the entire file
    if (is_gz) {
        // Read from zcat pipe
        std::string cmd = "zcat \"";
        cmd += path;
        cmd += "\" 2>/dev/null";
        FILE* fp = popen(cmd.c_str(), "r");
        if (!fp) {
            std::cerr << "Error: cannot open gzipped trace file: " << path << "\n";
            return false;
        }

        char buf[512];
        while (fgets(buf, sizeof(buf), fp)) {
            std::string line(buf);
            if (is_hex) {
                int page;
                if (parseHexTraceLine(line, page_size, page))
                    trace.push_back(page);
            } else {
                // Plain integer per line
                int page;
                if (sscanf(line.c_str(), "%d", &page) == 1)
                    trace.push_back(page);
            }
        }
        pclose(fp);
    } else {
        std::ifstream file(path.c_str());
        if (!file.is_open()) {
            std::cerr << "Error: cannot open trace file: " << path << "\n";
            return false;
        }

        if (is_hex) {
            std::string line;
            while (std::getline(file, line)) {
                int page;
                if (parseHexTraceLine(line, page_size, page))
                    trace.push_back(page);
            }
        } else {
            int page;
            while (file >> page) {
                trace.push_back(page);
            }
        }
        file.close();
    }

    if (trace.empty()) {
        std::cerr << "Error: trace file is empty or unreadable: " << path << "\n";
        return false;
    }
    std::cout << "Read " << trace.size() << " page references from " << path;
    if (is_hex) std::cout << " (hex format, page_size=" << page_size << ")";
    std::cout << "\n";
    return true;
}

// ============================================================================
// Simulator data types
// ============================================================================

struct SimResult {
    std::string algo_name;
    std::string trace_name;
    int         frames;
    size_t      page_faults;
    size_t      total_refs;
    double      fault_rate;
};

// ============================================================================
// Algorithm factory (avoids templates in main loop)
// ============================================================================

void* createAlgorithm(int algo_idx, int num_frames) {
    switch (algo_idx) {
    case 0: return new page::LruCounter(num_frames);
    case 1: return new page::LruStack(num_frames);
    case 2: return new page::ArbAlgorithm(num_frames);
    case 3: return new page::SecondChance(num_frames);
    default: return NULL;
    }
}

void destroyAlgorithm(int algo_idx, void* algo) {
    switch (algo_idx) {
    case 0: delete static_cast<page::LruCounter*>(algo); break;
    case 1: delete static_cast<page::LruStack*>(algo); break;
    case 2: delete static_cast<page::ArbAlgorithm*>(algo); break;
    case 3: delete static_cast<page::SecondChance*>(algo); break;
    }
}

std::string getAlgorithmName(int algo_idx) {
    switch (algo_idx) {
    case 0: return "LRU Counter";
    case 1: return "LRU Stack";
    case 2: return "ARB";
    case 3: return "Second Chance";
    default: return "Unknown";
    }
}

bool runReference(void* algo, int algo_idx, int page) {
    switch (algo_idx) {
    case 0: return static_cast<page::LruCounter*>(algo)->reference(page);
    case 1: return static_cast<page::LruStack*>(algo)->reference(page);
    case 2: return static_cast<page::ArbAlgorithm*>(algo)->reference(page);
    case 3: return static_cast<page::SecondChance*>(algo)->reference(page);
    default: return false;
    }
}

size_t getFaults(void* algo, int algo_idx) {
    switch (algo_idx) {
    case 0: return static_cast<page::LruCounter*>(algo)->getPageFaults();
    case 1: return static_cast<page::LruStack*>(algo)->getPageFaults();
    case 2: return static_cast<page::ArbAlgorithm*>(algo)->getPageFaults();
    case 3: return static_cast<page::SecondChance*>(algo)->getPageFaults();
    default: return 0;
    }
}

// ============================================================================
// Output formatting
// ============================================================================

void printSeparator(const std::vector<int>& col_widths) {
    std::cout << "+";
    for (size_t i = 0; i < col_widths.size(); ++i) {
        for (int w = 0; w < col_widths[i] + 2; w++)
            std::cout << "-";
        std::cout << "+";
    }
    std::cout << "\n";
}

void printTable(const std::vector<SimResult>& results,
                const std::vector<int>& frame_counts,
                const std::vector<std::string>& trace_names)
{
    const int algo_count = 4;

    for (size_t t = 0; t < trace_names.size(); ++t) {
        const std::string& tname = trace_names[t];

        // Find total_refs for this trace (from any result)
        size_t total = 0;
        for (size_t r = 0; r < results.size(); ++r) {
            if (results[r].trace_name == tname) {
                total = results[r].total_refs;
                break;
            }
        }

        std::cout << "\n============================================================\n";
        std::cout << "  Trace: " << tname << "  (" << total << " references)\n";
        std::cout << "============================================================\n\n";

        // Column: Algorithm | Fr=N (faults / rate%) | ...
        // Two sub-columns per frame: fault count + rate
        std::vector<int> col_w;
        col_w.push_back(16);  // Algorithm name
        for (size_t f = 0; f < frame_counts.size(); ++f)
            col_w.push_back(18);  // "XXXX (XX.XX%)"

        printSeparator(col_w);
        std::cout << "| " << std::left  << std::setw(16) << "Algorithm";
        for (size_t f = 0; f < frame_counts.size(); ++f) {
            std::ostringstream hdr;
            hdr << "Fr=" << frame_counts[f];
            std::cout << " | " << std::right << std::setw(18) << hdr.str();
        }
        std::cout << " |\n";
        printSeparator(col_w);

        for (int a = 0; a < algo_count; ++a) {
            std::cout << "| " << std::left << std::setw(16) << getAlgorithmName(a);
            for (size_t f = 0; f < frame_counts.size(); ++f) {
                double rate = -1.0;
                size_t faults = 0;
                for (size_t r = 0; r < results.size(); ++r) {
                    if (results[r].trace_name == tname &&
                        results[r].algo_name == getAlgorithmName(a) &&
                        results[r].frames == frame_counts[f]) {
                        rate = results[r].fault_rate;
                        faults = results[r].page_faults;
                        break;
                    }
                }
                if (rate >= 0.0) {
                    std::ostringstream oss;
                    oss << faults << " (" << std::fixed << std::setprecision(1) << rate << "%)";
                    std::cout << " | " << std::right << std::setw(18) << oss.str();
                } else {
                    std::cout << " | " << std::right << std::setw(18) << "N/A";
                }
            }
            std::cout << " |\n";
        }
        printSeparator(col_w);
    }
}

void printBarChart(const std::vector<SimResult>& results,
                   const std::vector<int>& frame_counts,
                   const std::vector<std::string>& trace_names)
{
    const int algo_count = 4;
    const int bar_width = 50;

    for (size_t t = 0; t < trace_names.size(); ++t) {
        for (size_t f = 0; f < frame_counts.size(); ++f) {
            // Find max rate for scaling this (trace, frames) combo
            double max_rate = 0.0;
            for (int a = 0; a < algo_count; ++a) {
                for (size_t r = 0; r < results.size(); ++r) {
                    if (results[r].trace_name == trace_names[t] &&
                        results[r].algo_name == getAlgorithmName(a) &&
                        results[r].frames == frame_counts[f]) {
                        if (results[r].fault_rate > max_rate)
                            max_rate = results[r].fault_rate;
                    }
                }
            }
            if (max_rate < 0.01) max_rate = 100.0;

            std::cout << "\n-- Trace: " << trace_names[t]
                      << "  |  Frames = " << frame_counts[f] << " --\n";

            for (int a = 0; a < algo_count; ++a) {
                double rate = -1.0;
                size_t faults = 0;
                for (size_t r = 0; r < results.size(); ++r) {
                    if (results[r].trace_name == trace_names[t] &&
                        results[r].algo_name == getAlgorithmName(a) &&
                        results[r].frames == frame_counts[f]) {
                        rate = results[r].fault_rate;
                        faults = results[r].page_faults;
                        break;
                    }
                }
                int filled = static_cast<int>((rate / max_rate) * bar_width + 0.5);
                if (filled < 1 && rate > 0.0) filled = 1;

                std::ostringstream oss;
                oss << faults << " (" << std::fixed << std::setprecision(1) << rate << "%)";

                std::cout << "  " << std::left << std::setw(15) << getAlgorithmName(a)
                          << " |" << std::string(filled, '#')
                          << std::string(bar_width - filled, ' ')
                          << "| " << oss.str() << "\n";
            }
        }
    }
}

// ============================================================================
// Auto-scale frame counts based on trace properties
// ============================================================================

std::vector<int> autoFrameCounts(const std::vector<int>& trace) {
    // Find unique pages in trace (working set size)
    std::set<int> uniq(trace.begin(), trace.end());
    int ws_size = (int)uniq.size();

    std::vector<int> frames;
    // Generate frame counts at meaningful fractions of working set
    int candidates[] = {1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 16, 20, 24, 32, 48, 64, 128};

    for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); ++i) {
        int f = candidates[i];
        if (f <= ws_size + 10) {  // include up to slightly above working set
            frames.push_back(f);
        }
    }
    // Always include one frame count larger than working set
    if (frames.empty() || frames.back() < ws_size) {
        if (ws_size <= 64)
            frames.push_back(ws_size + (ws_size / 2));  // 1.5x working set
        else
            frames.push_back(std::min(128, ws_size + 20));
    }
    return frames;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    const int algo_count = 4;

    // Parse arguments
    std::string file_path;
    std::string trace_dir;
    bool do_benchmark = false;
    int manual_pages = -1, manual_space = -1;
    int page_size = 4096;
    int limit_refs = -1;  // -1 means unlimited
    std::vector<int> manual_frames;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            file_path = argv[++i];
        } else if (std::strcmp(argv[i], "--tracedir") == 0 && i + 1 < argc) {
            trace_dir = argv[++i];
        } else if (std::strcmp(argv[i], "--pagesize") == 0 && i + 1 < argc) {
            page_size = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--limit") == 0 && i + 1 < argc) {
            limit_refs = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            std::string fs = argv[++i];
            std::istringstream iss(fs);
            std::string tok;
            while (std::getline(iss, tok, ','))
                manual_frames.push_back(std::atoi(tok.c_str()));
        } else if (std::strcmp(argv[i], "--benchmark") == 0) {
            do_benchmark = true;
        } else if (std::strcmp(argv[i], "--pages") == 0 && i + 1 < argc) {
            manual_pages = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--space") == 0 && i + 1 < argc) {
            manual_space = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::cout << "Page Replacement Algorithm Simulator\n\n";
            std::cout << "Usage: " << argv[0] << " [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --file <path>        Read page trace from file\n";
            std::cout << "  --tracedir <path>    Read all .gz trace files from directory\n";
            std::cout << "  --pagesize <n>       Page size in bytes (default: 4096)\n";
            std::cout << "  --limit <n>          Limit references per trace (default: unlimited)\n";
            std::cout << "  --frames <n1,...>    Frame counts (default: auto-scaled)\n";
            std::cout << "  --benchmark          Run all built-in traces\n";
            std::cout << "  --pages <n>          References per synthetic trace (default: 1000)\n";
            std::cout << "  --space <n>          Max page number for synthetic traces (default: 30)\n";
            std::cout << "  --help               Show this help\n\n";
            std::cout << "Examples:\n";
            std::cout << "  " << argv[0] << " --file traces/gcc.log\n";
            std::cout << "  " << argv[0] << " --benchmark\n";
            std::cout << "  " << argv[0] << " --file traces/gcc.log --frames 4,8,16,32\n";
            std::cout << "  " << argv[0] << " --tracedir /path/to/traces\n";
            std::cout << "  " << argv[0] << " --tracedir /path/to/traces --pagesize 4096 --frames 8,16,32,64\n";
            std::cout << "  " << argv[0] << " --tracedir /path/to/traces --limit 100000\n";
            return 0;
        }
    }

    std::cout << "\n";
    std::cout << "+==========================================+\n";
    std::cout << "|  Page Replacement Algorithm Simulator    |\n";
    std::cout << "+==========================================+\n\n";

    // ================================================================
    // MODE 1: File-based trace
    // ================================================================
    if (!file_path.empty()) {
        std::vector<int> trace;
        if (!readTraceFile(file_path, trace, page_size))
            return 1;

        // Determine frame counts
        std::vector<int> frame_counts;
        if (!manual_frames.empty())
            frame_counts = manual_frames;
        else
            frame_counts = autoFrameCounts(trace);

        std::cout << "Frame counts: ";
        for (size_t i = 0; i < frame_counts.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << frame_counts[i];
        }
        std::cout << "\n\n";

        std::vector<SimResult> all_results;
        size_t required = trace.size();
        if (limit_refs > 0 && (size_t)limit_refs < required) {
            required = (size_t)limit_refs;
            std::cout << "Limited to first " << required << " references\n";
        }

        for (size_t f = 0; f < frame_counts.size(); ++f) {
            int frames = frame_counts[f];
            std::cout << "  Frames=" << frames << ": ";

            for (int a = 0; a < algo_count; ++a) {
                void* algo = createAlgorithm(a, frames);

                size_t tested = 0;
                while (tested < required) {
                    runReference(algo, a, trace[tested]);
                    tested++;
                }

                SimResult r;
                r.algo_name   = getAlgorithmName(a);
                r.trace_name  = file_path;
                r.frames      = frames;
                r.page_faults = getFaults(algo, a);
                r.total_refs  = required;
                r.fault_rate  = 100.0 * (double)r.page_faults / (double)r.total_refs;
                all_results.push_back(r);

                std::cout << getAlgorithmName(a) << "=" << r.page_faults << "  ";
                destroyAlgorithm(a, algo);
            }
            std::cout << "\n";
        }

        // Output
        std::vector<std::string> trace_names;
        trace_names.push_back(file_path);

        std::cout << "\n";
        std::cout << "+-------------------------------------------+\n";
        std::cout << "|  RESULTS: Page Faults (count + rate)      |\n";
        std::cout << "+-------------------------------------------+\n";
        printTable(all_results, frame_counts, trace_names);

        std::cout << "\n";
        std::cout << "+-------------------------------------------+\n";
        std::cout << "|  Bar Charts                               |\n";
        std::cout << "+-------------------------------------------+\n";
        printBarChart(all_results, frame_counts, trace_names);

        std::cout << "\nDone.\n";
        return 0;
    }

    // ================================================================
    // MODE 3: Trace directory — batch-run all .gz traces
    // ================================================================
    if (!trace_dir.empty()) {
        // Collect all .gz files from the directory
        std::vector<std::string> gz_files;
        DIR* dir = opendir(trace_dir.c_str());
        if (!dir) {
            std::cerr << "Error: cannot open trace directory: " << trace_dir << "\n";
            return 1;
        }
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            std::string name(entry->d_name);
            if (hasGzExtension(name)) {
                std::string full_path = trace_dir;
                if (full_path.back() != '/') full_path += '/';
                full_path += name;
                gz_files.push_back(full_path);
            }
        }
        closedir(dir);

        if (gz_files.empty()) {
            std::cerr << "Error: no .gz files found in directory: " << trace_dir << "\n";
            return 1;
        }

        // Sort files for consistent output
        std::sort(gz_files.begin(), gz_files.end());

        std::cout << "Found " << gz_files.size() << " .gz trace file(s) in "
                  << trace_dir << "\n";
        std::cout << "Page size: " << page_size << " bytes\n\n";

        // Load all traces
        struct TraceInfo {
            std::string name;
            std::vector<int> data;
        };
        std::vector<TraceInfo> traces;

        for (size_t i = 0; i < gz_files.size(); ++i) {
            std::vector<int> tdata;
            std::cout << "[" << (i+1) << "/" << gz_files.size() << "] Loading ";
            if (!readTraceFile(gz_files[i], tdata, page_size)) {
                std::cerr << "Warning: skipping file: " << gz_files[i] << "\n";
                continue;
            }
            TraceInfo ti;
            // Use basename without .gz extension as trace name
            std::string fname = gz_files[i];
            size_t slash_pos = fname.find_last_of('/');
            if (slash_pos != std::string::npos)
                fname = fname.substr(slash_pos + 1);
            if (hasGzExtension(fname))
                fname = fname.substr(0, fname.size() - 3);
            ti.name = fname;
            ti.data = tdata;
            traces.push_back(ti);
        }

        if (traces.empty()) {
            std::cerr << "Error: no valid traces loaded.\n";
            return 1;
        }

        // Determine frame counts
        std::vector<int> frame_counts;
        if (!manual_frames.empty()) {
            frame_counts = manual_frames;
        } else {
            int max_ws = 0;
            for (size_t t = 0; t < traces.size(); ++t) {
                std::set<int> uniq(traces[t].data.begin(), traces[t].data.end());
                max_ws = std::max(max_ws, (int)uniq.size());
            }
            int candidates[] = {1,2,3,4,5,6,7,8,10,12,14,16,20,24,28,32,40,48,56,64,96,128};
            for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); ++i) {
                if (candidates[i] <= max_ws + 16)
                    frame_counts.push_back(candidates[i]);
            }
            if (!frame_counts.empty() && frame_counts.back() < max_ws + 4) {
                frame_counts.push_back(max_ws + 4);
            }
        }

        std::cout << "\nConfiguration:\n";
        std::cout << "  Traces:         " << traces.size() << "\n";
        std::cout << "  Frame counts:   ";
        for (size_t i = 0; i < frame_counts.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << frame_counts[i];
        }
        std::cout << "\n\n";

        // Run simulations
        std::vector<SimResult> all_results;

        for (size_t t = 0; t < traces.size(); ++t) {
            const std::string& tname = traces[t].name;
            const std::vector<int>& tdata = traces[t].data;
            size_t required = tdata.size();
            if (limit_refs > 0 && (size_t)limit_refs < required) {
                required = (size_t)limit_refs;
            }

            std::cout << "Trace: " << tname << " (" << required << " refs";
            if (required < tdata.size()) std::cout << ", limited from " << tdata.size();
            std::cout << ")\n";

            for (size_t f = 0; f < frame_counts.size(); ++f) {
                int frames = frame_counts[f];
                std::cout << "  Fr=" << frames << ": ";

                for (int a = 0; a < algo_count; ++a) {
                    void* algo = createAlgorithm(a, frames);

                    size_t tested = 0;
                    while (tested < required) {
                        runReference(algo, a, tdata[tested]);
                        tested++;
                    }

                    SimResult r;
                    r.algo_name   = getAlgorithmName(a);
                    r.trace_name  = tname;
                    r.frames      = frames;
                    r.page_faults = getFaults(algo, a);
                    r.total_refs  = required;
                    r.fault_rate  = 100.0 * (double)r.page_faults / (double)r.total_refs;
                    all_results.push_back(r);

                    std::cout << getAlgorithmName(a) << "=" << r.page_faults << "  ";
                    destroyAlgorithm(a, algo);
                }
                std::cout << "\n";
            }
        }

        // Extract trace names
        std::vector<std::string> trace_names;
        for (size_t t = 0; t < traces.size(); ++t)
            trace_names.push_back(traces[t].name);

        // Output tables
        std::cout << "\n";
        std::cout << "+-------------------------------------------+\n";
        std::cout << "|  RESULTS: Page Faults (count + rate)      |\n";
        std::cout << "+-------------------------------------------+\n";
        printTable(all_results, frame_counts, trace_names);

        // Bar charts
        std::cout << "\n";
        std::cout << "+-------------------------------------------+\n";
        std::cout << "|  Bar Charts                               |\n";
        std::cout << "+-------------------------------------------+\n";
        printBarChart(all_results, frame_counts, trace_names);

        std::cout << "\nDone.\n";
        return 0;
    }

    // ================================================================
    // MODE 2: Built-in traces
    // ================================================================

    int num_refs   = (manual_pages > 0) ? manual_pages : 1000;
    int max_page   = (manual_space > 0) ? manual_space : 30;

    struct TraceInfo {
        std::string name;
        std::vector<int> data;
    };
    std::vector<TraceInfo> traces;

    if (do_benchmark) {
        // Full benchmark: all traces
        traces.push_back({"Classic1 (Silberschatz)", getClassicTrace1()});
        traces.push_back({"Classic2 (Multi-phase)", getClassicTrace2()});

        TraceGenerator gen(num_refs, max_page);
        traces.push_back({"TightLoop (95% in 4pp)", gen.generateTightLoop()});
        traces.push_back({"Program (code+data)",     gen.generateProgram()});
        traces.push_back({"Locality (80% hot)",      gen.generateLocality()});
        traces.push_back({"Sequential (no locality)", gen.generateSequential()});
        traces.push_back({"Random (no locality)",    gen.generateRandom()});
    } else {
        // Quick mode: traces ordered by locality strength
        traces.push_back({"Classic1 (OS textbook)", getClassicTrace1()});
        traces.push_back({"Classic2 (Multi-phase)", getClassicTrace2()});

        TraceGenerator gen(num_refs, max_page);
        traces.push_back({"TightLoop",              gen.generateTightLoop()});
        traces.push_back({"Program (code+data)",    gen.generateProgram()});
        traces.push_back({"Locality (80% hot)",     gen.generateLocality()});
        traces.push_back({"Sequential (baseline)",  gen.generateSequential()});
    }

    // Auto-determine frame counts from the trace with largest working set
    std::vector<int> frame_counts;
    if (!manual_frames.empty()) {
        frame_counts = manual_frames;
    } else {
        int max_ws = 0;
        for (size_t t = 0; t < traces.size(); ++t) {
            std::set<int> uniq(traces[t].data.begin(), traces[t].data.end());
            max_ws = std::max(max_ws, (int)uniq.size());
        }
        // Build frame list around the max working set size
        int candidates[] = {1,2,3,4,5,6,7,8,10,12,14,16,20,24,28,32,40,48,56,64};
        for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); ++i) {
            if (candidates[i] <= max_ws + 16)
                frame_counts.push_back(candidates[i]);
        }
        // Ensure we go beyond working set size
        if (!frame_counts.empty() && frame_counts.back() < max_ws + 4) {
            frame_counts.push_back(max_ws + 4);
        }
    }

    std::cout << "Configuration:\n";
    std::cout << "  Max page refs:  " << num_refs << " per synthetic trace\n";
    std::cout << "  Address space:  " << max_page << " pages\n";
    std::cout << "  Frame counts:   ";
    for (size_t i = 0; i < frame_counts.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << frame_counts[i];
    }
    std::cout << "\n\n";

    // Run simulations
    std::vector<SimResult> all_results;

    for (size_t t = 0; t < traces.size(); ++t) {
        const std::string& tname = traces[t].name;
        const std::vector<int>& tdata = traces[t].data;
        size_t required = tdata.size();

        std::cout << "Trace: " << tname << " (" << required << " refs)\n";

        for (size_t f = 0; f < frame_counts.size(); ++f) {
            int frames = frame_counts[f];
            std::cout << "  Fr=" << frames << ": ";

            for (int a = 0; a < algo_count; ++a) {
                void* algo = createAlgorithm(a, frames);

                size_t tested = 0;
                while (tested < required) {
                    runReference(algo, a, tdata[tested]);
                    tested++;
                }

                SimResult r;
                r.algo_name   = getAlgorithmName(a);
                r.trace_name  = tname;
                r.frames      = frames;
                r.page_faults = getFaults(algo, a);
                r.total_refs  = required;
                r.fault_rate  = 100.0 * (double)r.page_faults / (double)r.total_refs;
                all_results.push_back(r);

                std::cout << getAlgorithmName(a) << "=" << r.page_faults << "  ";
                destroyAlgorithm(a, algo);
            }
            std::cout << "\n";
        }
    }

    // Extract trace names
    std::vector<std::string> trace_names;
    for (size_t t = 0; t < traces.size(); ++t)
        trace_names.push_back(traces[t].name);

    // Table
    std::cout << "\n";
    std::cout << "+-------------------------------------------+\n";
    std::cout << "|  RESULTS: Page Faults (count + rate)      |\n";
    std::cout << "+-------------------------------------------+\n";
    printTable(all_results, frame_counts, trace_names);

    // Bar charts
    std::cout << "\n";
    std::cout << "+-------------------------------------------+\n";
    std::cout << "|  Bar Charts                               |\n";
    std::cout << "+-------------------------------------------+\n";
    printBarChart(all_results, frame_counts, trace_names);

    // Summary
    std::cout << "\n";
    std::cout << "+-------------------------------------------+\n";
    std::cout << "|  SUMMARY: Average Across All Traces       |\n";
    std::cout << "+-------------------------------------------+\n\n";

    std::vector<int> col_w;
    col_w.push_back(16);
    for (size_t f = 0; f < frame_counts.size(); ++f)
        col_w.push_back(18);

    printSeparator(col_w);
    std::cout << "| " << std::left  << std::setw(16) << "Algorithm";
    for (size_t f = 0; f < frame_counts.size(); ++f) {
        std::ostringstream hdr;
        hdr << "Fr=" << frame_counts[f];
        std::cout << " | " << std::right << std::setw(18) << hdr.str();
    }
    std::cout << " |\n";
    printSeparator(col_w);

    for (int a = 0; a < algo_count; ++a) {
        std::cout << "| " << std::left << std::setw(16) << getAlgorithmName(a);
        for (size_t f = 0; f < frame_counts.size(); ++f) {
            double sum = 0.0;
            int cnt = 0;
            for (size_t r = 0; r < all_results.size(); ++r) {
                if (all_results[r].algo_name == getAlgorithmName(a) &&
                    all_results[r].frames == frame_counts[f]) {
                    sum += all_results[r].fault_rate;
                    cnt++;
                }
            }
            double avg = (cnt > 0) ? sum / cnt : 0.0;
            size_t avg_faults = (size_t)(avg / 100.0 * num_refs);
            std::ostringstream oss;
            oss << "~" << avg_faults << " (" << std::fixed << std::setprecision(1) << avg << "%)";
            std::cout << " | " << std::right << std::setw(18) << oss.str();
        }
        std::cout << " |\n";
    }
    printSeparator(col_w);

    std::cout << "\nDone.\n";
    return 0;
}

