// Command-line front end for the ParcelHub benchmark engine.
// Exit codes: 0 = all results verified, 1 = a verification failure, 2 = bad command line.
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "Benchmark.h"
#include "BenchmarkExport.h"

namespace {

void usage(std::ostream& out) {
    out << "Usage: parcelhub_bench [options]\n"
           "  --profile quick|standard   preset sizes and repeats (default: quick)\n"
           "  --seed N                   base seed; the same seed reproduces the same inputs (default 20240601)\n"
           "  --repeats N                timed runs per series (default from profile)\n"
           "  --warmups N                untimed warm-up runs per series (default 1)\n"
           "  --instances N              independent random inputs per size (default from profile)\n"
           "  --sizes a,b,c              hub counts for the Dijkstra/Prim graphs\n"
           "  --parcels a,b,c            parcel counts for the dispatch and merge benchmarks\n"
           "  --densities sparse,medium,dense\n"
           "  --heaps binary,binomial,fibonacci\n"
           "  --benchmarks dijkstra,prim,dispatch,merge\n"
           "  --max-dense N              skip dense graphs with more than N hubs (default 2000)\n"
           "  --out DIR                  where benchmark_results.csv/.json are written (default results)\n"
           "  --no-export                print the summary only\n"
           "  --quiet                    no progress lines\n"
           "  --help\n";
}

std::vector<std::string> splitList(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) if (!item.empty()) out.push_back(item);
    if (out.empty()) throw std::invalid_argument("empty list");
    return out;
}

long long parseNumber(const std::string& s, const std::string& flag) {
    try {
        std::size_t used = 0;
        long long v = std::stoll(s, &used);
        if (used != s.size()) throw std::invalid_argument("");
        return v;
    } catch (const std::exception&) {
        throw std::invalid_argument(flag + " expects a whole number, got '" + s + "'");
    }
}

std::vector<int> parseSizes(const std::string& s, const std::string& flag) {
    std::vector<int> out;
    for (const std::string& item : splitList(s)) {
        long long v = parseNumber(item, flag);
        if (v < 2 || v > 5000000) throw std::invalid_argument(flag + " values must be between 2 and 5000000");
        out.push_back(static_cast<int>(v));
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::vector<std::string> args(argv + 1, argv + argc);
        std::string profile = "quick", outDir = "results";
        bool exportFiles = true, quiet = false;
        // Collect overrides first so a profile given later on the line does not erase them.
        std::string seed, repeats, warmups, instances, sizes, parcels, densities, heaps, benchmarks, maxDense;

        auto value = [&](std::size_t& i) -> std::string {
            if (i + 1 >= args.size()) throw std::invalid_argument(args[i] + " needs a value");
            return args[++i];
        };
        for (std::size_t i = 0; i < args.size(); ++i) {
            const std::string& a = args[i];
            if (a == "--help" || a == "-h") { usage(std::cout); return 0; }
            else if (a == "--profile") profile = value(i);
            else if (a == "--seed") seed = value(i);
            else if (a == "--repeats") repeats = value(i);
            else if (a == "--warmups") warmups = value(i);
            else if (a == "--instances") instances = value(i);
            else if (a == "--sizes") sizes = value(i);
            else if (a == "--parcels") parcels = value(i);
            else if (a == "--densities") densities = value(i);
            else if (a == "--heaps") heaps = value(i);
            else if (a == "--benchmarks") benchmarks = value(i);
            else if (a == "--max-dense") maxDense = value(i);
            else if (a == "--out") outDir = value(i);
            else if (a == "--no-export") exportFiles = false;
            else if (a == "--quiet") quiet = true;
            else throw std::invalid_argument("unknown option '" + a + "'");
        }

        BenchConfig cfg = profileConfig(profile);
        if (!seed.empty()) {
            long long v = parseNumber(seed, "--seed");
            if (v < 0) throw std::invalid_argument("--seed must not be negative");
            cfg.seed = static_cast<uint64_t>(v);
        }
        if (!repeats.empty()) cfg.repeats = static_cast<int>(parseNumber(repeats, "--repeats"));
        if (!warmups.empty()) cfg.warmups = static_cast<int>(parseNumber(warmups, "--warmups"));
        if (!instances.empty()) cfg.instances = static_cast<int>(parseNumber(instances, "--instances"));
        if (!maxDense.empty()) cfg.maxDenseNodes = static_cast<int>(parseNumber(maxDense, "--max-dense"));
        if (!sizes.empty()) cfg.graphSizes = parseSizes(sizes, "--sizes");
        if (!parcels.empty()) cfg.parcelSizes = parseSizes(parcels, "--parcels");
        if (!densities.empty()) {
            cfg.densities.clear();
            for (const std::string& d : splitList(densities)) {
                auto p = parseDensity(d);
                if (!p) throw std::invalid_argument("unknown density '" + d + "' (sparse, medium, dense)");
                cfg.densities.push_back(*p);
            }
        }
        if (!heaps.empty()) {
            cfg.heaps.clear();
            for (const std::string& h : splitList(heaps)) {
                auto p = parseHeapType(h);
                if (!p) throw std::invalid_argument("unknown heap '" + h + "' (binary, binomial, fibonacci)");
                cfg.heaps.push_back(*p);
            }
        }
        if (!benchmarks.empty()) {
            cfg.runDijkstra = cfg.runPrim = cfg.runDispatch = cfg.runMerge = false;
            for (const std::string& b : splitList(benchmarks)) {
                if (b == "dijkstra") cfg.runDijkstra = true;
                else if (b == "prim") cfg.runPrim = true;
                else if (b == "dispatch") cfg.runDispatch = true;
                else if (b == "merge") cfg.runMerge = true;
                else throw std::invalid_argument("unknown benchmark '" + b + "' (dijkstra, prim, dispatch, merge)");
            }
        }

        std::cout << "ParcelHub benchmark - profile " << cfg.profile << ", seed " << cfg.seed << ", " << cfg.repeats
                  << " timed runs (+" << cfg.warmups << " warm-up) x " << cfg.instances << " instance(s)\n";
        BenchReport report = runBenchmarks(cfg, quiet ? nullptr : &std::cout);
        std::cout << "\n";
        printSummary(report, std::cout);

        if (exportFiles) {
            std::string err;
            std::string csv = outDir + "/benchmark_results.csv", json = outDir + "/benchmark_results.json";
            if (!writeTextFile(csv, toCsv(report), err) || !writeTextFile(json, toJson(report), err)) {
                std::cerr << "Export failed: " << err << "\n";
                return 1;
            }
            std::cout << "Wrote " << csv << " and " << json << "\n";
        }
        return report.ok() ? 0 : 1;
    } catch (const std::invalid_argument& e) {
        std::cerr << "Error: " << e.what() << "\n\n";
        usage(std::cerr);
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "Benchmark aborted: " << e.what() << "\n";
        return 1;
    }
}
