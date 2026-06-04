/*
 * File:   page_simulator.cpp
 * Page replacement algorithm simulator.
 *
 * Simulates four page replacement algorithms (LRU Counter, LRU Stack,
 * Additional-Reference-Bits, Second Chance) on various memory access
 * traces, and outputs results as tables and ASCII bar charts.
 *
 * Usage:
 *   ./page_simulator [--frames 2,4,8,16,32] [--pages 10000] [--trace all]
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <cmath>

#include "lru_counter.hpp"
#include "lru_stack.hpp"
#include "arb_algorithm.hpp"
#include "second_chance.hpp"

// ============================================================================
// Trace Generator
// ============================================================================

class TraceGenerator {
public:
    TraceGenerator(int num_pages = 10000, int address_space = 200)
        : _num_pages(num_pages), _address_space(address_space)
    {
        std::srand(42); // fixed seed for reproducibility
    }

    // 1. Sequential scan: 0,1,2,...,N-1, 0,1,2,... (repeating)
    std::vector<int> generateSequential() {
        std::vector<int> trace;
        trace.reserve(_num_pages);
        for (int i = 0; i < _num_pages; ++i) {
            trace.push_back(i % _address_space);
        }
        return trace;
    }

    // 2. Hot loop: repeatedly access a small working set (simulates a tight loop)
    std::vector<int> generateHotLoop() {
        std::vector<int> trace;
        trace.reserve(_num_pages);
        int loop_size = _address_space / 10; // small working set
        for (int i = 0; i < _num_pages; ++i) {
            trace.push_back(i % loop_size);
        }
        return trace;
    }

    // 3. Random access: uniformly random in address space
    std::vector<int> generateRandom() {
        std::vector<int> trace;
        trace.reserve(_num_pages);
        for (int i = 0; i < _num_pages; ++i) {
            trace.push_back(std::rand() % _address_space);
        }
        return trace;
    }

    // 4. Locality-based: 80% accesses in 20% of address space (Pareto/zipf-like)
    std::vector<int> generateLocality() {
        std::vector<int> trace;
        trace.reserve(_num_pages);
        int hot_set = _address_space / 5;  // 20% hot pages
        int cold_set = _address_space - hot_set;
        for (int i = 0; i < _num_pages; ++i) {
            if ((std::rand() % 100) < 80) {
                // 80% of accesses go to hot pages
                trace.push_back(std::rand() % hot_set);
            } else {
                // 20% go to cold pages
                trace.push_back(hot_set + (std::rand() % cold_set));
            }
        }
        return trace;
    }

    // 5. Mixed: sequential phases + random phases + loops
    std::vector<int> generateMixed() {
        std::vector<int> trace;
        trace.reserve(_num_pages);
        int section_size = _num_pages / 5;
        for (int s = 0; s < 5; ++s) {
            switch (s) {
            case 0: // sequential
                for (int i = 0; i < section_size; ++i)
                    trace.push_back(i % _address_space);
                break;
            case 1: // random
                for (int i = 0; i < section_size; ++i)
                    trace.push_back(std::rand() % _address_space);
                break;
            case 2: // hot loop
                for (int i = 0; i < section_size; ++i)
                    trace.push_back(i % (_address_space / 10));
                break;
            case 3: // locality
                for (int i = 0; i < section_size; ++i) {
                    if ((std::rand() % 100) < 80)
                        trace.push_back(std::rand() % (_address_space / 5));
                    else
                        trace.push_back(_address_space / 5 + (std::rand() % (_address_space * 4 / 5)));
                }
                break;
            case 4: // sequential reverse
                for (int i = 0; i < section_size; ++i)
                    trace.push_back((_address_space - 1) - (i % _address_space));
                break;
            }
        }
        return trace;
    }

private:
    int _num_pages;
    int _address_space;
};

// ============================================================================
// Simulator
// ============================================================================

struct SimResult {
    std::string algo_name;
    std::string trace_name;
    int         frames;
    size_t      page_faults;
    size_t      total_refs;
    double      fault_rate;
};

// Factory: create algorithm by index
void* createAlgorithm(int algo_idx, int num_frames) {
    switch (algo_idx) {
    case 0: return new page::LruCounter(num_frames);
    case 1: return new page::LruStack(num_frames);
    case 2: return new page::ArbAlgorithm(num_frames);
    case 3: return new page::SecondChance(num_frames);
    default: return nullptr;
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
        std::cout << std::string(col_widths[i] + 2, '-') << "+";
    }
    std::cout << "\n";
}

void printTable(const std::vector<SimResult>& results,
                const std::vector<int>& frame_counts,
                const std::vector<std::string>& trace_names)
{
    const int algo_count = 4;

    for (size_t t = 0; t < trace_names.size(); ++t) {
        std::cout << "\n";
        std::cout << "══════════════════════════════════════════════════════════\n";
        std::cout << "  Trace: " << trace_names[t] << "\n";
        std::cout << "══════════════════════════════════════════════════════════\n\n";

        // Build column widths
        std::vector<int> col_widths;
        int label_w = 16;
        col_widths.push_back(label_w);
        for (size_t f = 0; f < frame_counts.size(); ++f) {
            col_widths.push_back(10);
        }

        // Header
        printSeparator(col_widths);
        std::cout << "| " << std::left << std::setw(label_w) << "Algorithm";
        for (size_t f = 0; f < frame_counts.size(); ++f) {
            std::string hdr = "Fr=" + std::to_string(frame_counts[f]);
            std::cout << " | " << std::right << std::setw(10) << hdr;
        }
        std::cout << " |\n";
        printSeparator(col_widths);

        // Data rows
        for (int a = 0; a < algo_count; ++a) {
            std::cout << "| " << std::left << std::setw(label_w) << getAlgorithmName(a);
            for (size_t f = 0; f < frame_counts.size(); ++f) {
                // Find matching result
                double rate = -1.0;
                for (size_t r = 0; r < results.size(); ++r) {
                    if (results[r].trace_name == trace_names[t] &&
                        results[r].algo_name == getAlgorithmName(a) &&
                        results[r].frames == frame_counts[f]) {
                        rate = results[r].fault_rate;
                        break;
                    }
                }
                if (rate >= 0.0) {
                    std::ostringstream oss;
                    oss << std::fixed << std::setprecision(2) << rate << "%";
                    std::cout << " | " << std::right << std::setw(10) << oss.str();
                } else {
                    std::cout << " | " << std::right << std::setw(10) << "N/A";
                }
            }
            std::cout << " |\n";
        }
        printSeparator(col_widths);
    }
}

void printBarChart(const std::vector<SimResult>& results,
                   const std::vector<int>& frame_counts,
                   const std::vector<std::string>& trace_names)
{
    const int algo_count = 4;
    const int bar_width = 60;

    for (size_t t = 0; t < trace_names.size(); ++t) {
        for (size_t f = 0; f < frame_counts.size(); ++f) {
            std::cout << "\n┌─ Trace: " << trace_names[t]
                      << "  |  Frames = " << frame_counts[f] << " ─┐\n";

            // Find max rate for scaling
            double max_rate = 0.0;
            for (int a = 0; a < algo_count; ++a) {
                for (size_t r = 0; r < results.size(); ++r) {
                    if (results[r].trace_name == trace_names[t] &&
                        results[r].algo_name == getAlgorithmName(a) &&
                        results[r].frames == frame_counts[f]) {
                        max_rate = std::max(max_rate, results[r].fault_rate);
                    }
                }
            }
            if (max_rate < 0.01) max_rate = 100.0; // avoid div by zero

            for (int a = 0; a < algo_count; ++a) {
                double rate = -1.0;
                for (size_t r = 0; r < results.size(); ++r) {
                    if (results[r].trace_name == trace_names[t] &&
                        results[r].algo_name == getAlgorithmName(a) &&
                        results[r].frames == frame_counts[f]) {
                        rate = results[r].fault_rate;
                        break;
                    }
                }

                int filled = static_cast<int>((rate / max_rate) * bar_width);

                std::ostringstream oss;
                oss << std::fixed << std::setprecision(2) << rate << "%";

                std::cout << "│ " << std::left << std::setw(15) << getAlgorithmName(a)
                          << " │" << std::string(filled, '#')
                          << std::string(bar_width - filled, ' ')
                          << "│ " << std::right << std::setw(7) << oss.str() << " │\n";
            }
            std::cout << "└──────────────────────────────────────────────────────────────────────────────┘\n";
        }
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    // Default parameters
    std::vector<int> frame_counts = {2, 4, 8, 16, 32, 64};
    int num_pages = 10000;
    int address_space = 200;
    std::string trace_filter = "all";

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frame_counts.clear();
            std::string frames_str = argv[++i];
            std::istringstream iss(frames_str);
            std::string token;
            while (std::getline(iss, token, ',')) {
                frame_counts.push_back(std::atoi(token.c_str()));
            }
        } else if (std::strcmp(argv[i], "--pages") == 0 && i + 1 < argc) {
            num_pages = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--space") == 0 && i + 1 < argc) {
            address_space = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--trace") == 0 && i + 1 < argc) {
            trace_filter = argv[++i];
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::cout << "Page Replacement Algorithm Simulator\n\n";
            std::cout << "Usage: " << argv[0] << " [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --frames <n1,n2,...>   Frame counts to test (default: 2,4,8,16,32,64)\n";
            std::cout << "  --pages <n>            Number of page references per trace (default: 10000)\n";
            std::cout << "  --space <n>            Address space size (default: 200)\n";
            std::cout << "  --trace <name>         Trace to run: sequential, hotloop, random, locality, mixed, all (default: all)\n";
            std::cout << "  --help                 Show this help\n";
            return 0;
        }
    }

    const int algo_count = 4;

    // Generate traces
    TraceGenerator gen(num_pages, address_space);

    struct TraceInfo {
        std::string name;
        std::vector<int> data;
    };

    std::vector<TraceInfo> traces;
    if (trace_filter == "all" || trace_filter == "sequential")
        traces.push_back({"Sequential", gen.generateSequential()});
    if (trace_filter == "all" || trace_filter == "hotloop")
        traces.push_back({"HotLoop", gen.generateHotLoop()});
    if (trace_filter == "all" || trace_filter == "random")
        traces.push_back({"Random", gen.generateRandom()});
    if (trace_filter == "all" || trace_filter == "locality")
        traces.push_back({"Locality", gen.generateLocality()});
    if (trace_filter == "all" || trace_filter == "mixed")
        traces.push_back({"Mixed", gen.generateMixed()});

    std::cout << "\n╔══════════════════════════════════════════════╗\n";
    std::cout << "║   Page Replacement Algorithm Simulator       ║\n";
    std::cout << "╚══════════════════════════════════════════════╝\n";
    std::cout << "\nConfiguration:\n";
    std::cout << "  Pages per trace: " << num_pages << "\n";
    std::cout << "  Address space:   " << address_space << " pages\n";
    std::cout << "  Frame counts:    ";
    for (size_t i = 0; i < frame_counts.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << frame_counts[i];
    }
    std::cout << "\n";
    std::cout << "  Traces:          ";
    for (size_t i = 0; i < traces.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << traces[i].name << " (" << traces[i].data.size() << " refs)";
    }
    std::cout << "\n\n";

    // Collect all results
    std::vector<SimResult> all_results;

    // Run simulation for each trace
    for (size_t t = 0; t < traces.size(); ++t) {
        const std::string& trace_name = traces[t].name;
        const std::vector<int>& trace_data = traces[t].data;

        std::cout << "Running trace: " << trace_name << " (" << trace_data.size() << " references)...\n";

        for (size_t f = 0; f < frame_counts.size(); ++f) {
            int frames = frame_counts[f];

            for (int a = 0; a < algo_count; ++a) {
                void* algo = createAlgorithm(a, frames);

                // Main test loop
                size_t tested = 0;
                size_t required = trace_data.size();
                while (tested < required) {
                    int page = trace_data[tested];
                    runReference(algo, a, page);
                    tested++;
                }

                SimResult result;
                result.algo_name  = getAlgorithmName(a);
                result.trace_name = trace_name;
                result.frames     = frames;
                result.page_faults = getFaults(algo, a);
                result.total_refs = trace_data.size();
                result.fault_rate = (double)result.page_faults / result.total_refs * 100.0;
                all_results.push_back(result);

                destroyAlgorithm(a, algo);
            }
        }
    }

    // Extract trace names
    std::vector<std::string> trace_names;
    for (size_t t = 0; t < traces.size(); ++t) {
        trace_names.push_back(traces[t].name);
    }

    // Print results table
    std::cout << "\n\n";
    std::cout << "┌───────────────────────────────────────────┐\n";
    std::cout << "│  RESULTS: Page Fault Rate (%)             │\n";
    std::cout << "└───────────────────────────────────────────┘\n";
    printTable(all_results, frame_counts, trace_names);

    // Print ASCII bar charts
    std::cout << "\n\n";
    std::cout << "┌───────────────────────────────────────────┐\n";
    std::cout << "│  RESULTS: Bar Charts                      │\n";
    std::cout << "└───────────────────────────────────────────┘\n";
    printBarChart(all_results, frame_counts, trace_names);

    // Summary: average across all traces
    std::cout << "\n\n";
    std::cout << "┌───────────────────────────────────────────┐\n";
    std::cout << "│  SUMMARY: Average Across All Traces       │\n";
    std::cout << "└───────────────────────────────────────────┘\n\n";

    std::vector<int> col_w;
    col_w.push_back(16);
    for (size_t f = 0; f < frame_counts.size(); ++f)
        col_w.push_back(10);

    printSeparator(col_w);
    std::cout << "| " << std::left << std::setw(16) << "Algorithm";
    for (size_t f = 0; f < frame_counts.size(); ++f) {
        std::string hdr = "Fr=" + std::to_string(frame_counts[f]);
        std::cout << " | " << std::right << std::setw(10) << hdr;
    }
    std::cout << " |\n";
    printSeparator(col_w);

    for (int a = 0; a < algo_count; ++a) {
        std::cout << "| " << std::left << std::setw(16) << getAlgorithmName(a);
        for (size_t f = 0; f < frame_counts.size(); ++f) {
            double sum = 0.0;
            int count = 0;
            for (size_t r = 0; r < all_results.size(); ++r) {
                if (all_results[r].algo_name == getAlgorithmName(a) &&
                    all_results[r].frames == frame_counts[f]) {
                    sum += all_results[r].fault_rate;
                    count++;
                }
            }
            double avg = (count > 0) ? sum / count : 0.0;
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << avg << "%";
            std::cout << " | " << std::right << std::setw(10) << oss.str();
        }
        std::cout << " |\n";
    }
    printSeparator(col_w);

    std::cout << "\nDone.\n";
    return 0;
}
