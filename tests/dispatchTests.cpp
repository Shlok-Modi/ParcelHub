#include <memory>
#include <set>
#include <vector>
#include "BinaryHeap.h"
#include "DispatchEngine.h"
#include "Graph.h"
#include "TestUtil.h"

static DispatchEngine makeEngine(const Graph& g) { return DispatchEngine(g, std::make_unique<BinaryHeap>()); }

int main() {
    Graph g = makeSampleGraph();

    // Express < Same-day < Standard, FIFO within a level
    {
        auto e = makeEngine(g);
        e.addParcel(makeParcel(1, 1, 5, ServiceLevel::Standard, 0, 0));
        e.addParcel(makeParcel(2, 1, 5, ServiceLevel::Express,  0, 1));
        e.addParcel(makeParcel(3, 1, 5, ServiceLevel::SameDay,  0, 2));
        e.addParcel(makeParcel(4, 1, 5, ServiceLevel::Express,  0, 3));
        e.addParcel(makeParcel(5, 1, 5, ServiceLevel::Standard, 0, 4));
        std::vector<int> order;
        while (auto r = e.dispatchNext()) order.push_back(r->parcel.id);
        CHECK((order == std::vector<int>{2, 4, 3, 1, 5}));
    }

    // Same priority and same arrival: parcel id is the stable tie-break
    {
        auto e = makeEngine(g);
        e.addParcel(makeParcel(9, 1, 2, ServiceLevel::Express, 0, 0));
        e.addParcel(makeParcel(4, 1, 2, ServiceLevel::Express, 0, 0));
        CHECK(e.dispatchNext()->parcel.id == 4);
        CHECK(e.dispatchNext()->parcel.id == 9);
    }

    // Upgrade reorders the queue; invalid upgrades are rejected and change nothing
    {
        auto e = makeEngine(g);
        e.addParcel(makeParcel(1, 1, 5, ServiceLevel::SameDay,  0, 0));
        e.addParcel(makeParcel(2, 1, 5, ServiceLevel::Standard, 0, 1));
        CHECK(e.peekNext()->id == 1);
        e.upgradePriority(2, ServiceLevel::Express);
        CHECK(e.peekNext()->id == 2);
        CHECK(e.find(2)->level == ServiceLevel::Express && e.find(2)->priority == 1);
        CHECK_THROWS(e.upgradePriority(2, ServiceLevel::Express));    // same level
        CHECK_THROWS(e.upgradePriority(1, ServiceLevel::Standard));   // downgrade
        CHECK_THROWS(e.upgradePriority(77, ServiceLevel::Express));   // unknown id
        CHECK(e.find(1)->priority == 2);
        e.dispatchNext();
        CHECK_THROWS(e.upgradePriority(2, ServiceLevel::Express));    // already dispatched
    }

    // Validation
    {
        auto e = makeEngine(g);
        e.addParcel(makeParcel(1, 1, 5, ServiceLevel::Express, 0, 0));
        CHECK_THROWS(e.addParcel(makeParcel(1, 1, 5, ServiceLevel::Express, 0, 1)));   // duplicate id
        CHECK_THROWS(e.addParcel(makeParcel(2, 1, 99, ServiceLevel::Express, 0, 1)));  // bad hub
        CHECK_THROWS(e.addParcel(makeParcel(3, 0, 5, ServiceLevel::Express, 0, 1)));   // bad hub
        CHECK(e.totalCount() == 1 && e.pendingCount() == 1);
    }

    // Empty queue
    { auto e = makeEngine(g); CHECK(!e.dispatchNext().has_value()); CHECK(e.peekNext() == nullptr); }

    // Routing from dispatch + unreachable destination + counters, nothing lost or duplicated
    {
        auto e = makeEngine(g);
        e.addParcel(makeParcel(1, 1, 7, ServiceLevel::Express,  0, 0));
        e.addParcel(makeParcel(2, 1, 8, ServiceLevel::SameDay,  0, 1));
        e.addParcel(makeParcel(3, 6, 4, ServiceLevel::Standard, 0, 2));
        auto a = e.dispatchNext();
        CHECK(a->reachable && a->cost == 23 && (a->path == std::vector<int>{1, 3, 6, 5, 7}));
        auto b = e.dispatchNext();
        CHECK(!b->reachable && b->path.empty());
        auto c = e.dispatchNext();
        CHECK(c->reachable && c->cost == 13 && (c->path == std::vector<int>{6, 3, 4}));
        CHECK(e.pendingCount() == 0 && e.dispatchedCount() == 3);
        CHECK(e.find(1)->status == Status::Dispatched);
    }

    // Larger run: every parcel dispatched exactly once
    {
        auto e = makeEngine(g);
        for (int i = 1; i <= 500; ++i)
            e.addParcel(makeParcel(i, 1 + i % 7, 1 + (i * 3) % 7, static_cast<ServiceLevel>(1 + i % 3), 0, i));
        for (int i = 3; i <= 500; i += 7)
            if (e.find(i)->priority > 1) e.upgradePriority(i, ServiceLevel::Express);
        std::set<int> seen; bool ok = true; int lastPri = 0;
        while (auto r = e.dispatchNext()) {
            ok = ok && seen.insert(r->parcel.id).second;
            ok = ok && r->parcel.priority >= lastPri;
            lastPri = r->parcel.priority;
        }
        CHECK(ok); CHECK(seen.size() == 500);
    }
    return finishTests("dispatchTests");
}
