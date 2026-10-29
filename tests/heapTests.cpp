// The same behavioural suite runs against Binary, Binomial and Fibonacci heaps,
// followed by structure-specific checks.
#include <algorithm>
#include <random>
#include <vector>
#include "BinaryHeap.h"
#include "BinomialHeap.h"
#include "FibonacciHeap.h"
#include "HeapFactory.h"
#include "TestUtil.h"

static bool byKey(const HeapEntry& a, const HeapEntry& b) { return a.key < b.key; }

static void commonSuite(HeapType type) {
    testContext() = toString(type);

    // Empty heap
    { auto h = makeHeap(type); CHECK(h->empty()); CHECK_THROWS(h->extractMin()); CHECK_THROWS(h->peekMin());
      CHECK_THROWS(h->decreaseKey(1, {0, 0, 1})); CHECK(h->checkInvariants()); }

    // Single item
    { auto h = makeHeap(type); h->insert(7, {5, 0, 7}); CHECK(h->size() == 1); CHECK(h->contains(7));
      CHECK(h->peekMin().id == 7); CHECK(h->extractMin().id == 7); CHECK(h->empty()); CHECK(!h->contains(7)); }

    // Tie-breaking through the composite key
    {
        auto h = makeHeap(type);
        h->insert(3, {1, 5, 3}); h->insert(1, {1, 2, 1}); h->insert(2, {1, 2, 2});
        CHECK(h->extractMin().id == 1); CHECK(h->extractMin().id == 2); CHECK(h->extractMin().id == 3);
    }

    // Duplicate id rejected, heap unchanged
    { auto h = makeHeap(type); h->insert(1, {1, 0, 1}); CHECK_THROWS(h->insert(1, {0, 0, 1}));
      CHECK(h->size() == 1); CHECK(h->peekMin().key.primary == 1); CHECK(h->checkInvariants()); }

    // Identical keys (only the id differs in the tertiary slot)
    {
        auto h = makeHeap(type);
        for (int i = 0; i < 50; ++i) h->insert(i, {4, 4, 4});
        int n = 0; while (!h->empty()) { h->extractMin(); ++n; }
        CHECK(n == 50);
    }

    // Randomised: extraction order equals sorted order
    {
        std::mt19937 rng(12345);
        auto h = makeHeap(type);
        std::vector<HeapEntry> ref;
        for (int i = 0; i < 2000; ++i) {
            Key k{(long long)(rng() % 50), (long long)(rng() % 50), i};
            h->insert(i, k); ref.push_back({i, k});
            if (i % 250 == 0) CHECK(h->checkInvariants());
        }
        CHECK(h->checkInvariants());
        std::sort(ref.begin(), ref.end(), byKey);
        bool ok = true;
        for (std::size_t i = 0; i < ref.size(); ++i) {
            ok = ok && (h->extractMin().id == ref[i].id);
            if (i % 300 == 0) CHECK(h->checkInvariants());
        }
        CHECK(ok); CHECK(h->empty());
        CHECK(h->counters().inserts == 2000 && h->counters().extracts == 2000);
    }

    // decrease-key basics
    {
        auto h = makeHeap(type);
        for (int i = 1; i <= 10; ++i) h->insert(i, {10 + i, 0, i});
        h->extractMin();   // forces tree restructuring before decrease-key (id 1 leaves)
        h->decreaseKey(9, {1, 0, 9});
        CHECK(h->peekMin().id == 9); CHECK(h->checkInvariants());
        h->decreaseKey(9, {1, 0, 9});                      // equal key is allowed
        CHECK_THROWS(h->decreaseKey(9, {50, 0, 9}));       // increase rejected
        CHECK_THROWS(h->decreaseKey(99, {0, 0, 99}));      // unknown id
        CHECK(h->extractMin().id == 9);
        CHECK(h->extractMin().id == 2);
        CHECK(h->counters().decreaseKeys == 2);
    }

    // Randomised decrease-key with invariant checks after every operation batch
    {
        std::mt19937 rng(777);
        auto h = makeHeap(type);
        std::vector<Key> cur(600);
        for (int i = 0; i < 600; ++i) { cur[i] = {(long long)(1000 + rng() % 1000), 0, i}; h->insert(i, cur[i]); }
        for (int i = 0; i < 40; ++i) h->extractMin();           // consolidate / restructure
        std::vector<char> gone(600, 0);
        // the first 40 extracted are the smallest keys; recompute which ids are gone
        {
            std::vector<HeapEntry> ref;
            for (int i = 0; i < 600; ++i) ref.push_back({i, cur[i]});
            std::sort(ref.begin(), ref.end(), byKey);
            for (int i = 0; i < 40; ++i) gone[ref[i].id] = 1;
        }
        for (int t = 0; t < 500; ++t) {
            int id = rng() % 600;
            if (gone[id]) continue;
            Key nk{cur[id].primary - (long long)(rng() % 400), 0, id};
            h->decreaseKey(id, nk); cur[id] = nk;
            if (t % 25 == 0) CHECK(h->checkInvariants());
        }
        CHECK(h->checkInvariants());
        std::vector<HeapEntry> ref;
        for (int i = 0; i < 600; ++i) if (!gone[i]) ref.push_back({i, cur[i]});
        std::sort(ref.begin(), ref.end(), byKey);
        bool ok = h->size() == ref.size();
        for (const auto& e : ref) ok = ok && (h->extractMin().id == e.id);
        CHECK(ok); CHECK(h->empty());
    }

    // Merge: nothing lost or duplicated, order correct, other emptied
    {
        auto a = makeHeap(type), b = makeHeap(type);
        for (int i = 0; i < 10; ++i) a->insert(i, {i * 2, 0, i});
        for (int i = 10; i < 20; ++i) b->insert(i, {i * 2 - 19, 0, i});
        a->extractMin(); b->extractMin();     // make the structures non-trivial first
        a->mergeFrom(*b);
        CHECK(a->size() == 18); CHECK(b->empty()); CHECK(b->checkInvariants()); CHECK(a->checkInvariants());
        long long prev = -1000; bool sorted = true; int n = 0;
        while (!a->empty()) { auto e = a->extractMin(); sorted = sorted && e.key.primary >= prev; prev = e.key.primary; ++n; }
        CHECK(sorted); CHECK(n == 18);
    }

    // Merge with empty heaps; rejected merges leave both heaps untouched
    {
        auto a = makeHeap(type), empty = makeHeap(type);
        a->insert(1, {1, 0, 1});
        a->mergeFrom(*empty); CHECK(a->size() == 1);
        empty->mergeFrom(*a); CHECK(empty->size() == 1 && a->empty());
        auto x = makeHeap(type), y = makeHeap(type);
        x->insert(5, {1, 0, 5}); y->insert(5, {2, 0, 5}); y->insert(6, {3, 0, 6});
        CHECK_THROWS(x->mergeFrom(*y));
        CHECK(x->size() == 1 && y->size() == 2 && x->checkInvariants() && y->checkInvariants());
        CHECK_THROWS(x->mergeFrom(*x));
        auto other = makeHeap(type == HeapType::Binary ? HeapType::Fibonacci : HeapType::Binary);
        other->insert(100, {1, 0, 100});
        CHECK_THROWS(x->mergeFrom(*other));                 // different heap type
        CHECK(x->size() == 1 && other->size() == 1);
    }

    // Repeated merges (many small heaps into one)
    {
        auto big = makeHeap(type);
        int id = 0; std::mt19937 rng(99);
        std::vector<long long> all;
        for (int round = 0; round < 40; ++round) {
            auto small = makeHeap(type);
            int k = 1 + rng() % 12;
            for (int j = 0; j < k; ++j) { long long p = rng() % 500; small->insert(id, {p, 0, id}); all.push_back(p); ++id; }
            big->mergeFrom(*small);
            CHECK(big->checkInvariants());
            if (round % 7 == 0) { all.erase(std::min_element(all.begin(), all.end())); big->extractMin(); }
        }
        std::sort(all.begin(), all.end());
        bool ok = big->size() == all.size();
        for (long long p : all) ok = ok && big->extractMin().key.primary == p;
        CHECK(ok);
    }
}

static void binomialSpecific() {
    testContext() = "Binomial Heap (structure)";
    // After n inserts the number of roots equals popcount(n): the binary representation of n.
    BinomialHeap h;
    for (int n = 1; n <= 200; ++n) {
        h.insert(n, {(long long)((n * 37) % 101), 0, n});
        CHECK(h.rootCount() == (std::size_t)__builtin_popcount(n));
        CHECK(h.checkInvariants());
    }
    // union of two heaps: roots follow binary addition of the sizes
    BinomialHeap a, b;
    for (int i = 0; i < 13; ++i) a.insert(i, {i, 0, i});
    for (int i = 100; i < 111; ++i) b.insert(i, {i, 0, i});
    a.merge(b);
    CHECK(a.size() == 24 && a.rootCount() == (std::size_t)__builtin_popcount(24));
    CHECK(a.checkInvariants());
}

static void fibonacciSpecific() {
    testContext() = "Fibonacci Heap (structure)";
    // Build a deep structure, then force cuts and cascading cuts.
    std::mt19937 rng(4242);
    FibonacciHeap h;
    const int N = 3000;
    std::vector<Key> cur(N);
    for (int i = 0; i < N; ++i) { cur[i] = {(long long)(100000 + rng() % 100000), 0, i}; h.insert(i, cur[i]); }
    CHECK(h.cuts() == 0);
    std::vector<char> gone(N, 0);
    std::vector<HeapEntry> ref;
    for (int i = 0; i < N; ++i) ref.push_back({i, cur[i]});
    std::sort(ref.begin(), ref.end(), byKey);
    h.extractMin(); gone[ref[0].id] = 1;                 // one extract consolidates 3000 roots into trees
    CHECK(h.checkInvariants());

    long long before = h.counters().decreaseKeys;
    for (int t = 0; t < 2500; ++t) {
        int id = rng() % N;
        if (gone[id]) continue;
        Key nk{cur[id].primary - (long long)(1 + rng() % 5000), 0, id};
        h.decreaseKey(id, nk); cur[id] = nk;
        if (t % 10 == 0) CHECK(h.checkInvariants());
    }
    CHECK(h.counters().decreaseKeys > before);
    CHECK(h.cuts() > 0);
    CHECK(h.cascadingCuts() > 0);                         // marks were set, then triggered cascading cuts
    CHECK(h.checkInvariants());

    ref.clear();
    for (int i = 0; i < N; ++i) if (!gone[i]) ref.push_back({i, cur[i]});
    std::sort(ref.begin(), ref.end(), byKey);
    bool ok = h.size() == ref.size();
    for (const auto& e : ref) ok = ok && h.extractMin().id == e.id;
    CHECK(ok);

    // meld is a structural splice: min pointer must come from whichever heap has the smaller key
    FibonacciHeap x, y;
    x.insert(1, {50, 0, 1}); y.insert(2, {10, 0, 2}); y.insert(3, {60, 0, 3});
    x.meld(y);
    CHECK(x.peekMin().id == 2 && x.size() == 3 && y.empty() && x.checkInvariants());
}

int main() {
    for (HeapType t : kAllHeapTypes) commonSuite(t);
    binomialSpecific();
    fibonacciSpecific();
    return finishTests("heapTests");
}
