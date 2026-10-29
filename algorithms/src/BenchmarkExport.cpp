#include "BenchmarkExport.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
std::string num(double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.6f", v);
    return buf;
}
std::string optionalInt(long long v) { return v < 0 ? "" : std::to_string(v); }
}  // namespace

std::string csvEscape(const std::string& f) {
    if (f.find_first_of(",\"\r\n") == std::string::npos) return f;
    std::string out = "\"";
    for (char c : f) { if (c == '"') out += '"'; out += c; }
    return out + "\"";
}

std::string jsonEscape(const std::string& text) {
    std::string out;
    for (unsigned char c : text) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof buf, "\\u%04x", c); out += buf; }
                else out += static_cast<char>(c);
        }
    }
    return out;
}

std::string toCsv(const BenchReport& report) {
    std::ostringstream s;
    s << "benchmark,heap,density,size,edges,upgrades,instance,seed,repeats,"
         "mean_ms,median_ms,min_ms,max_ms,stddev_ms,inserts,extracts,decrease_keys,merges,comparisons,verified\n";
    for (const BenchRow& r : report.rows) {
        s << csvEscape(r.benchmark) << ',' << csvEscape(r.heap) << ',' << csvEscape(r.density) << ',' << r.size << ','
          << optionalInt(r.edges) << ',' << optionalInt(r.upgrades) << ',' << r.instance << ',' << r.seed << ',' << r.repeats << ','
          << num(r.timeMs.mean) << ',' << num(r.timeMs.median) << ',' << num(r.timeMs.min) << ',' << num(r.timeMs.max) << ','
          << num(r.timeMs.stddev) << ',' << r.ops.inserts << ',' << r.ops.extracts << ',' << r.ops.decreaseKeys << ','
          << r.ops.merges << ',' << r.ops.comparisons << ",true\n";
    }
    return s.str();
}

std::string toJson(const BenchReport& report) {
    const BenchMeta& m = report.meta;
    std::ostringstream s;
    s << "{\n  \"meta\": {\n"
      << "    \"profile\": \"" << jsonEscape(m.profile) << "\",\n"
      << "    \"seed\": " << m.seed << ",\n"
      << "    \"repeats\": " << m.repeats << ",\n"
      << "    \"warmups\": " << m.warmups << ",\n"
      << "    \"instances\": " << m.instances << ",\n"
      << "    \"max_weight\": " << m.maxWeight << ",\n"
      << "    \"upgrade_fraction\": " << num(m.upgradeFraction) << ",\n"
      << "    \"compiler\": \"" << jsonEscape(m.compiler) << "\",\n"
      << "    \"optimized\": " << (m.optimized ? "true" : "false") << ",\n"
      << "    \"timestamp_utc\": \"" << jsonEscape(m.timestampUtc) << "\",\n"
      << "    \"time_unit\": \"ms\"\n  },\n  \"rows\": [";
    for (std::size_t i = 0; i < report.rows.size(); ++i) {
        const BenchRow& r = report.rows[i];
        s << (i ? ",\n" : "\n") << "    {\"benchmark\": \"" << jsonEscape(r.benchmark) << "\", \"heap\": \"" << jsonEscape(r.heap)
          << "\", \"density\": " << (r.density.empty() ? "null" : "\"" + jsonEscape(r.density) + "\"")
          << ", \"size\": " << r.size << ", \"edges\": " << (r.edges < 0 ? "null" : std::to_string(r.edges))
          << ", \"upgrades\": " << (r.upgrades < 0 ? "null" : std::to_string(r.upgrades))
          << ", \"instance\": " << r.instance << ", \"seed\": " << r.seed << ", \"repeats\": " << r.repeats
          << ",\n     \"time_ms\": {\"mean\": " << num(r.timeMs.mean) << ", \"median\": " << num(r.timeMs.median)
          << ", \"min\": " << num(r.timeMs.min) << ", \"max\": " << num(r.timeMs.max) << ", \"stddev\": " << num(r.timeMs.stddev)
          << "},\n     \"ops\": {\"inserts\": " << r.ops.inserts << ", \"extracts\": " << r.ops.extracts
          << ", \"decrease_keys\": " << r.ops.decreaseKeys << ", \"merges\": " << r.ops.merges
          << ", \"comparisons\": " << r.ops.comparisons << "},\n     \"verified\": true}";
    }
    s << (report.rows.empty() ? "" : "\n  ") << "],\n  \"failures\": [";
    for (std::size_t i = 0; i < report.failures.size(); ++i) {
        const BenchFailure& f = report.failures[i];
        s << (i ? ",\n" : "\n") << "    {\"benchmark\": \"" << jsonEscape(f.benchmark) << "\", \"heap\": \"" << jsonEscape(f.heap)
          << "\", \"density\": " << (f.density.empty() ? "null" : "\"" + jsonEscape(f.density) + "\"") << ", \"size\": " << f.size
          << ", \"instance\": " << f.instance << ", \"seed\": " << f.seed << ", \"message\": \"" << jsonEscape(f.message) << "\"}";
    }
    s << (report.failures.empty() ? "" : "\n  ") << "]\n}\n";
    return s.str();
}

bool writeTextFile(const std::string& path, const std::string& text, std::string& error) {
    try {
        std::filesystem::path p(path);
        if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f) { error = "cannot open " + path + " for writing"; return false; }
        f << text;
        f.flush();
        if (!f) { error = "write to " + path + " failed"; return false; }
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
