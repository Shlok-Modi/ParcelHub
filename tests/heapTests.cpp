#include <algorithm>
#include <random>
#include <vector>
#include "BinaryHeap.h"
#include "TestUtil.h"

int main() {
    // Empty heap
    { BinaryHeap h; CHECK(h.empty()); CHECK_THROWS(h.extractMin()); CHECK_THROWS(h.peekMin()); }

    // Single item
    { BinaryHeap h; h.insert(7, {5, 0, 7}); CHECK(h.size() == 1); CHECK(h.extractMin().id == 7); CHECK(h.empty()); }

    // Duplicate priorities resolved by secondary then tertiary key
    {
        BinaryHeap h;
        h.insert(3, {1, 5, 3});
        h.insert(1, {1, 2, 1});
        h.insert(2, {1, 2, 2});
        CHECK(h.extractMin().id == 1);
        CHECK(h.extractMin().id == 2);
        CHECK(h.extractMin().id == 3);
    }

    // Duplicate id rejected
    { BinaryHeap h; h.insert(1, {1, 0, 1}); CHECK_THROWS(h.insert(1, {0, 0, 1})); CHECK(h.size() == 1); }

    // Randomised: extraction order equals sorted order (fixed seed)
    {
        std::mt19937 rng(12345);
        BinaryHeap h;
        std::vector<HeapEntry> ref;
        for (int i = 0; i < 2000; ++i) {
            Key k{(long long)(rng() % 50), (long long)(rng() % 50), i};
            h.insert(i, k);
            ref.push_back({i, k});
        }
        std::sort(ref.begin(), ref.end(), [](const HeapEntry& a, const HeapEntry& b) { return a.key < b.key; });
        bool ok = true;
        for (const auto& e : ref) ok = ok && (h.extractMin().id == e.id);
        CHECK(ok);
        CHECK(h.empty());
    }

    // decrease-key moves an item to the front; increasing is rejected; unknown id rejected
    {
        BinaryHeap h;
        for (int i = 1; i <= 10; ++i) h.insert(i, {10 + i, 0, i});
        h.decreaseKey(9, {1, 0, 9});
        CHECK(h.peekMin().id == 9);
        CHECK_THROWS(h.decreaseKey(9, {50, 0, 9}));
        CHECK_THROWS(h.decreaseKey(99, {0, 0, 99}));
        CHECK(h.extractMin().id == 9);
        CHECK(h.extractMin().id == 1);
    }

    // Randomised decrease-key keeps heap order correct
    {
        std::mt19937 rng(777);
        BinaryHeap h;
        std::vector<Key> cur(500);
        for (int i = 0; i < 500; ++i) { cur[i] = {(long long)(1000 + rng() % 1000), 0, i}; h.insert(i, cur[i]); }
        for (int t = 0; t < 300; ++t) {
            int id = rng() % 500;
            Key nk{cur[id].primary - (long long)(rng() % 500), 0, id};
            h.decreaseKey(id, nk);
            cur[id] = nk;
        }
        std::vector<HeapEntry> ref;
        for (int i = 0; i < 500; ++i) ref.push_back({i, cur[i]});
        std::sort(ref.begin(), ref.end(), [](const HeapEntry& a, const HeapEntry& b) { return a.key < b.key; });
        bool ok = true;
        for (const auto& e : ref) ok = ok && (h.extractMin().id == e.id);
        CHECK(ok);
    }

    // Merge: nothing lost or duplicated, order correct, other emptied
    {
        BinaryHeap a, b;
        for (int i = 0; i < 10; ++i) a.insert(i, {i * 2, 0, i});
        for (int i = 10; i < 20; ++i) b.insert(i, {i * 2 - 19, 0, i});
        a.merge(b);
        CHECK(a.size() == 20); CHECK(b.empty());
        long long prev = -1; bool sorted = true; int n = 0;
        while (!a.empty()) { auto e = a.extractMin(); sorted = sorted && e.key.primary >= prev; prev = e.key.primary; ++n; }
        CHECK(sorted); CHECK(n == 20);
    }

    // Merge with empty heaps, and rejected merge leaves both heaps untouched
    {
        BinaryHeap a, empty;
        a.insert(1, {1, 0, 1});
        a.merge(empty); CHECK(a.size() == 1);
        empty.merge(a); CHECK(empty.size() == 1 && a.empty());
        BinaryHeap x, y;
        x.insert(5, {1, 0, 5}); y.insert(5, {2, 0, 5}); y.insert(6, {3, 0, 6});
        CHECK_THROWS(x.merge(y));
        CHECK(x.size() == 1 && y.size() == 2);
        CHECK_THROWS(x.merge(x));
    }

    return finishTests("heapTests");
}
