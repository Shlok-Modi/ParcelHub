#include "HeapFactory.h"
#include "BinaryHeap.h"
#include "BinomialHeap.h"
#include "FibonacciHeap.h"

std::unique_ptr<PriorityQueue> makeHeap(HeapType type) {
    switch (type) {
        case HeapType::Binomial:  return std::make_unique<BinomialHeap>();
        case HeapType::Fibonacci: return std::make_unique<FibonacciHeap>();
        default:                  return std::make_unique<BinaryHeap>();
    }
}

const char* toString(HeapType type) {
    switch (type) {
        case HeapType::Binomial:  return "Binomial Heap";
        case HeapType::Fibonacci: return "Fibonacci Heap";
        default:                  return "Binary Heap";
    }
}

std::optional<HeapType> parseHeapType(const std::string& name) {
    if (name == "binary")    return HeapType::Binary;
    if (name == "binomial")  return HeapType::Binomial;
    if (name == "fibonacci") return HeapType::Fibonacci;
    return std::nullopt;
}
