#pragma once
#include <memory>
#include <optional>
#include <string>
#include "PriorityQueue.h"

enum class HeapType { Binary, Binomial, Fibonacci };

inline const HeapType kAllHeapTypes[] = {HeapType::Binary, HeapType::Binomial, HeapType::Fibonacci};

std::unique_ptr<PriorityQueue> makeHeap(HeapType type);
const char* toString(HeapType type);
std::optional<HeapType> parseHeapType(const std::string& name);   // "binary" | "binomial" | "fibonacci"
