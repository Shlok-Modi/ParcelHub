#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

// Summary of repeated timing measurements (all values in the unit of the input).
struct Stats {
    int count = 0;
    double mean = 0, median = 0, min = 0, max = 0;
    double stddev = 0;   // sample standard deviation (n-1); 0 when fewer than 2 samples
};

inline Stats computeStats(std::vector<double> v) {
    Stats s;
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    s.count = static_cast<int>(v.size());
    s.min = v.front();
    s.max = v.back();
    double sum = 0;
    for (double x : v) sum += x;
    s.mean = sum / s.count;
    s.median = (s.count % 2) ? v[s.count / 2] : (v[s.count / 2 - 1] + v[s.count / 2]) / 2.0;
    if (s.count > 1) {
        double ss = 0;
        for (double x : v) ss += (x - s.mean) * (x - s.mean);
        s.stddev = std::sqrt(ss / (s.count - 1));
    }
    return s;
}
