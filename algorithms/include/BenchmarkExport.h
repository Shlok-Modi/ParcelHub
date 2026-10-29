#pragma once
#include <string>
#include "Benchmark.h"

// One header line plus one line per verified series. Times are in milliseconds.
std::string toCsv(const BenchReport& report);
// {"meta": {...}, "rows": [...], "failures": [...]}
std::string toJson(const BenchReport& report);

std::string csvEscape(const std::string& field);    // quotes the field if it needs it
std::string jsonEscape(const std::string& text);    // escapes quotes, backslashes, control characters

// Writes `text` to `path` (creating parent folders). Returns false and sets `error` on failure.
bool writeTextFile(const std::string& path, const std::string& text, std::string& error);
