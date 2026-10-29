#include "FibonacciHeap.h"
#include <stdexcept>
#include <utility>
#include <vector>

FibonacciHeap::~FibonacciHeap() {
    for (auto& kv : nodes_) delete kv.second;
}

// Inserts x next to min_ and updates min_. Resets x's parent and sibling links.
void FibonacciHeap::addToRootList(Node* x) {
    x->parent = nullptr;
    if (!min_) {
        x->left = x->right = x;
        min_ = x;
        return;
    }
    x->right = min_->right;
    x->left = min_;
    min_->right->left = x;
    min_->right = x;
    if (less(x->entry.key, min_->entry.key)) min_ = x;
}

void FibonacciHeap::link(Node* y, Node* x) {  // y becomes a child of x
    y->parent = x;
    y->mark = false;
    if (!x->child) {
        x->child = y;
        y->left = y->right = y;
    } else {
        Node* c = x->child;
        y->right = c->right;
        y->left = c;
        c->right->left = y;
        c->right = y;
    }
    ++x->degree;
}

void FibonacciHeap::insert(int id, const Key& key) {
    if (contains(id)) throw std::invalid_argument("FibonacciHeap::insert: duplicate id");
    Node* n = new Node(id, key);
    nodes_[id] = n;
    addToRootList(n);
    ++size_;
    ++c_.inserts;
}

const HeapEntry& FibonacciHeap::peekMin() const {
    if (!min_) throw std::out_of_range("FibonacciHeap::peekMin: heap is empty");
    return min_->entry;
}

void FibonacciHeap::consolidate() {
    std::vector<Node*> roots;
    Node* w = min_;
    do { roots.push_back(w); w = w->right; } while (w != min_);

    std::vector<Node*> byDegree;
    for (Node* x : roots) {
        std::size_t d = static_cast<std::size_t>(x->degree);
        for (;;) {
            if (d >= byDegree.size()) byDegree.resize(d + 1, nullptr);
            Node* y = byDegree[d];
            if (!y) break;
            if (less(y->entry.key, x->entry.key)) std::swap(x, y);
            link(y, x);
            byDegree[d] = nullptr;
            ++d;
        }
        byDegree[d] = x;
    }
    min_ = nullptr;
    for (Node* n : byDegree)
        if (n) addToRootList(n);
}

HeapEntry FibonacciHeap::extractMin() {
    Node* z = min_;
    if (!z) throw std::out_of_range("FibonacciHeap::extractMin: heap is empty");

    std::vector<Node*> kids;
    if (z->child) {
        Node* c = z->child;
        do { kids.push_back(c); c = c->right; } while (c != z->child);
        z->child = nullptr;
    }
    if (z->right == z) {
        min_ = nullptr;
    } else {
        z->left->right = z->right;
        z->right->left = z->left;
        min_ = z->right;   // provisional; consolidate() finds the real minimum
    }
    for (Node* k : kids) { k->mark = false; addToRootList(k); }
    if (min_) consolidate();

    HeapEntry result = z->entry;
    nodes_.erase(result.id);
    delete z;
    --size_;
    ++c_.extracts;
    return result;
}

void FibonacciHeap::cut(Node* x, Node* y) {
    if (x->right == x) {
        y->child = nullptr;
    } else {
        x->left->right = x->right;
        x->right->left = x->left;
        if (y->child == x) y->child = x->right;
    }
    --y->degree;
    x->mark = false;
    addToRootList(x);
    ++cuts_;
}

void FibonacciHeap::cascadingCut(Node* y) {
    while (y->parent) {
        Node* z = y->parent;
        if (!y->mark) { y->mark = true; break; }
        cut(y, z);
        ++cascades_;
        y = z;
    }
}

void FibonacciHeap::decreaseKey(int id, const Key& k) {
    auto it = nodes_.find(id);
    if (it == nodes_.end()) throw std::invalid_argument("FibonacciHeap::decreaseKey: unknown id");
    Node* x = it->second;
    if (x->entry.key < k) throw std::invalid_argument("FibonacciHeap::decreaseKey: new key is larger than current key");
    x->entry.key = k;
    Node* y = x->parent;
    if (y && less(x->entry.key, y->entry.key)) {
        cut(x, y);
        cascadingCut(y);
    }
    if (less(x->entry.key, min_->entry.key)) min_ = x;
    ++c_.decreaseKeys;
}

void FibonacciHeap::meld(FibonacciHeap& o) {
    if (&o == this) throw std::invalid_argument("FibonacciHeap::meld: cannot meld a heap with itself");
    for (const auto& kv : o.nodes_)
        if (contains(kv.first)) throw std::invalid_argument("FibonacciHeap::meld: duplicate id across heaps");
    if (o.min_) {
        Node* om = o.min_;
        if (!min_) {
            min_ = om;
        } else {
            Node* aR = min_->right;   // splice the two circular lists: O(1)
            Node* bL = om->left;
            min_->right = om;
            om->left = min_;
            bL->right = aR;
            aR->left = bL;
            if (less(om->entry.key, min_->entry.key)) min_ = om;
        }
    }
    nodes_.insert(o.nodes_.begin(), o.nodes_.end());
    size_ += o.size_;
    o.min_ = nullptr;
    o.nodes_.clear();
    o.size_ = 0;
    ++c_.merges;
}

void FibonacciHeap::mergeFrom(PriorityQueue& other) {
    FibonacciHeap* o = dynamic_cast<FibonacciHeap*>(&other);
    if (!o) throw std::invalid_argument("FibonacciHeap::mergeFrom: other heap is a different type");
    meld(*o);
}

bool FibonacciHeap::checkInvariants() const {
    if (!min_) return size_ == 0 && nodes_.empty();
    std::size_t count = 0;
    std::vector<const Node*> stack;
    const Node* r = min_;
    do {
        if (r->parent || r->mark) return false;
        if (r->right->left != r || r->left->right != r) return false;
        if (r->entry.key < min_->entry.key) return false;   // min_ really is minimal
        stack.push_back(r);
        r = r->right;
    } while (r != min_);

    while (!stack.empty()) {
        const Node* n = stack.back();
        stack.pop_back();
        ++count;
        auto it = nodes_.find(n->entry.id);
        if (it == nodes_.end() || it->second != n) return false;
        int kids = 0;
        if (n->child) {
            const Node* c = n->child;
            do {
                if (c->parent != n) return false;
                if (c->right->left != c || c->left->right != c) return false;
                if (c->entry.key < n->entry.key) return false;   // heap order
                ++kids;
                stack.push_back(c);
                c = c->right;
            } while (c != n->child);
        }
        if (kids != n->degree) return false;
    }
    return count == size_ && nodes_.size() == size_;
}
