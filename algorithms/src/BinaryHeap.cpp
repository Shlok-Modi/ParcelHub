#include "BinaryHeap.h"
#include <stdexcept>
#include <utility>

void BinaryHeap::swapAt(std::size_t i, std::size_t j) {
    std::swap(a_[i], a_[j]);
    pos_[a_[i].id] = i;
    pos_[a_[j].id] = j;
}

void BinaryHeap::siftUp(std::size_t i) {
    while (i > 0) {
        std::size_t p = (i - 1) / 2;
        if (!less(a_[i].key, a_[p].key)) break;
        swapAt(i, p);
        i = p;
    }
}

void BinaryHeap::siftDown(std::size_t i) {
    const std::size_t n = a_.size();
    for (;;) {
        std::size_t l = 2 * i + 1, r = l + 1, s = i;
        if (l < n && less(a_[l].key, a_[s].key)) s = l;
        if (r < n && less(a_[r].key, a_[s].key)) s = r;
        if (s == i) break;
        swapAt(i, s);
        i = s;
    }
}

void BinaryHeap::insert(int id, const Key& key) {
    if (contains(id)) throw std::invalid_argument("BinaryHeap::insert: duplicate id");
    a_.push_back({id, key});
    pos_[id] = a_.size() - 1;
    siftUp(a_.size() - 1);
    ++c_.inserts;
}

const HeapEntry& BinaryHeap::peekMin() const {
    if (a_.empty()) throw std::out_of_range("BinaryHeap::peekMin: heap is empty");
    return a_[0];
}

HeapEntry BinaryHeap::extractMin() {
    if (a_.empty()) throw std::out_of_range("BinaryHeap::extractMin: heap is empty");
    HeapEntry top = a_[0];
    swapAt(0, a_.size() - 1);
    pos_.erase(top.id);
    a_.pop_back();
    if (!a_.empty()) siftDown(0);
    ++c_.extracts;
    return top;
}

void BinaryHeap::decreaseKey(int id, const Key& k) {
    auto it = pos_.find(id);
    if (it == pos_.end()) throw std::invalid_argument("BinaryHeap::decreaseKey: unknown id");
    std::size_t i = it->second;
    if (a_[i].key < k) throw std::invalid_argument("BinaryHeap::decreaseKey: new key is larger than current key");
    a_[i].key = k;
    siftUp(i);
    ++c_.decreaseKeys;
}

void BinaryHeap::merge(BinaryHeap& other) {
    if (&other == this) throw std::invalid_argument("BinaryHeap::merge: cannot merge a heap with itself");
    for (const auto& e : other.a_)
        if (contains(e.id)) throw std::invalid_argument("BinaryHeap::merge: duplicate id across heaps");
    for (const auto& e : other.a_) {
        a_.push_back(e);
        pos_[e.id] = a_.size() - 1;
    }
    other.a_.clear();
    other.pos_.clear();
    for (std::size_t i = a_.size() / 2; i-- > 0;) siftDown(i);
    ++c_.merges;
}

void BinaryHeap::mergeFrom(PriorityQueue& other) {
    BinaryHeap* o = dynamic_cast<BinaryHeap*>(&other);
    if (!o) throw std::invalid_argument("BinaryHeap::mergeFrom: other heap is a different type");
    merge(*o);
}

bool BinaryHeap::checkInvariants() const {
    if (pos_.size() != a_.size()) return false;
    for (std::size_t i = 0; i < a_.size(); ++i) {
        auto it = pos_.find(a_[i].id);
        if (it == pos_.end() || it->second != i) return false;
        if (i > 0 && a_[i].key < a_[(i - 1) / 2].key) return false;
    }
    return true;
}
