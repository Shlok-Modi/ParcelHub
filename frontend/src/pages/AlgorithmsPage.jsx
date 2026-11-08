// A static explanation page: what each heap is, what it costs, and how Dijkstra, Prim and the
// dispatch queue use it. It shows no measured numbers on purpose; those live on the Benchmarks page.

const OPERATIONS = [
  { op: 'insert', binary: 'O(log n)', binomial: 'O(log n)', fibonacci: 'O(1)' },
  { op: 'extract-min', binary: 'O(log n)', binomial: 'O(log n)', fibonacci: 'O(log n)' },
  { op: 'decrease-key', binary: 'O(log n)', binomial: 'O(log n)', fibonacci: 'O(1)' },
  { op: 'merge', binary: 'O(n), by rebuilding', binomial: 'O(log n)', fibonacci: 'O(1)' },
];

const GRAPH_ALGORITHMS = [
  { name: 'Dijkstra (shortest paths)', binary: 'O((V + E) log V)', binomial: 'O((V + E) log V)', fibonacci: 'O(E + V log V)' },
  { name: 'Prim (minimum spanning tree)', binary: 'O((V + E) log V)', binomial: 'O((V + E) log V)', fibonacci: 'O(E + V log V)' },
];

const USES = [
  { who: 'Dispatch queue', how: 'Insert each parcel with the key (priority, arrival, id). Dispatch Next is extract-min. An upgrade is decrease-key. Merging two hubs is a heap merge.' },
  { who: 'Dijkstra', how: 'Insert the source. Extract the closest hub, then relax its roads: a shorter distance to a hub already in the queue is a decrease-key, otherwise an insert. There is at most one extract-min per hub and at most one decrease-key per road.' },
  { who: 'Prim', how: 'The same pattern, but the key is the cheapest road connecting a hub to the tree built so far. On a disconnected city it restarts in each part and returns a spanning forest.' },
];

function Complexity({ rows, firstHeader, nameKey }) {
  return (
    <div className="scroll">
      <table className="data">
        <thead>
          <tr>
            <th scope="col">{firstHeader}</th>
            <th scope="col">Binary Heap</th>
            <th scope="col">Binomial Heap</th>
            <th scope="col">Fibonacci Heap</th>
          </tr>
        </thead>
        <tbody>
          {rows.map((r) => (
            <tr key={r[nameKey]}>
              <th scope="row">{r[nameKey]}</th>
              <td>{r.binary}</td>
              <td>{r.binomial}</td>
              <td>{r.fibonacci}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

export default function AlgorithmsPage() {
  return (
    <div className="algo-page">
      <header className="algo-head">
        <h1>Algorithms</h1>
        <p className="stage-sub">
          ParcelHub needs one thing again and again: the smallest key, quickly. It picks the next parcel to dispatch,
          the closest unvisited hub in Dijkstra, and the cheapest road in Prim. Three priority queues are compared.
        </p>
      </header>

      <section className="algo-section" aria-labelledby="algo-costs">
        <h2 id="algo-costs">What each operation costs</h2>
        <Complexity rows={OPERATIONS} firstHeader="Operation" nameKey="op" />
        <p className="muted">
          Binary and Binomial costs are worst case. Fibonacci insert, decrease-key and merge are <strong>amortised</strong>:
          the cost averaged over a whole sequence of operations, so a single call can still be slow. Fibonacci merge is O(1) for
          joining the two root lists; this implementation also checks the ids and moves the other heap's id-to-node table, which takes time
          proportional to the other heap's size. n is the number of items in the heap.
        </p>
      </section>

      <section className="algo-section" aria-labelledby="algo-heaps">
        <h2 id="algo-heaps">How each heap works</h2>
        <div className="heap-cards">
          <article className="heap-card">
            <h3>Binary Heap</h3>
            <p>A complete binary tree stored in an array. The parent of position i is at (i − 1) / 2, and a parent never has a larger key than its children. The smallest key is always first.</p>
            <ul>
              <li><strong>Insert:</strong> add at the end, then sift up.</li>
              <li><strong>Extract-min:</strong> move the last item to the root, then sift down.</li>
              <li><strong>Decrease-key:</strong> find the item's position, lower its key, sift up.</li>
              <li><strong>Merge:</strong> append the other array and rebuild the heap bottom-up, which costs O(n).</li>
            </ul>
            <p className="muted">The baseline: simple, compact in memory, and fast in practice.</p>
          </article>

          <article className="heap-card">
            <h3>Binomial Heap</h3>
            <p>A list of binomial trees. A tree of degree k has 2<sup>k</sup> nodes, and the heap holds at most one tree of each degree, so a heap of 13 items has trees of sizes 8, 4 and 1 (13 is 1101 in binary).</p>
            <ul>
              <li><strong>Union:</strong> merge the two tree lists by degree. When two trees have the same degree, link them: the larger root goes under the smaller. This works like adding two binary numbers, so it costs O(log n).</li>
              <li><strong>Insert:</strong> a union with a one-item heap.</li>
              <li><strong>Extract-min:</strong> remove the smallest root. Its children form a heap of their own, which is united back in.</li>
              <li><strong>Decrease-key:</strong> sift up inside the tree, which is at most log n high.</li>
            </ul>
            <p className="muted">Its strength is fast merging: this is why it is used for combining hub queues.</p>
          </article>

          <article className="heap-card">
            <h3>Fibonacci Heap</h3>
            <p>A lazy heap. Trees sit in a circular root list with a pointer to the minimum. Most operations just add to the list and leave the tidying for later.</p>
            <ul>
              <li><strong>Insert and merge:</strong> add a one-node tree, or splice two root lists together. No restructuring.</li>
              <li><strong>Extract-min:</strong> promote the minimum's children to roots, then <em>consolidate</em>: link roots of equal degree until each degree appears once. This pays for all the earlier laziness.</li>
              <li><strong>Decrease-key:</strong> if the new key is smaller than the parent's, cut the node out to the root list instead of sifting. The parent is marked, and a cascading cut may follow (below).</li>
            </ul>
            <p className="muted">Cheap decrease-key is the point: Dijkstra and Prim call it often.</p>
          </article>
        </div>
      </section>

      <section className="algo-section" aria-labelledby="algo-cuts">
        <h2 id="algo-cuts">Cascading cuts</h2>
        <p>
          A node is <em>marked</em> when it has lost one child since it last became a child itself. Suppose a node x is decreased
          below its parent p:
        </p>
        <ol className="algo-steps">
          <li>Cut x from p and move it to the root list. Clear its mark.</li>
          <li>If p is a root, stop. If p is unmarked, mark it and stop: it has now lost one child.</li>
          <li>If p was already marked, this is its second lost child. Cut p as well and move it to the root list, then apply the same rule to p's parent.</li>
          <li>Continue upwards until an unmarked parent or a root is reached.</li>
        </ol>
        <p>
          The cuts can ripple up through several levels, hence the name. They are what keep the trees bushy: because no node loses more than one child
          before being cut itself, a node with k children always roots a subtree of at least F(k + 2) nodes (F is the Fibonacci sequence). Degrees therefore
          stay O(log n), which keeps extract-min at O(log n) amortised.
        </p>
      </section>

      <section className="algo-section" aria-labelledby="algo-amortised">
        <h2 id="algo-amortised">Why the costs are amortised</h2>
        <p>
          Use a potential function Φ = t + 2m, where t is the number of trees in the root list and m the number of marked nodes. The amortised
          cost of an operation is its actual cost plus the change in Φ. Cheap operations store up potential that expensive ones spend.
        </p>
        <ul className="algo-list">
          <li><strong>Insert:</strong> actual cost O(1). It adds one tree, so Φ rises by 1. Amortised O(1).</li>
          <li>
            <strong>Decrease-key:</strong> suppose it makes c cuts. The actual cost is O(c). Each cut adds a tree (+1), and each cascading cut also clears a mark
            (−2), so Φ changes by at most 4 − c. The cost and the change in Φ together are O(1), so decrease-key is amortised O(1).
          </li>
          <li>
            <strong>Extract-min:</strong> actual cost O(D(n) + t), where D(n) is the largest possible degree, O(log n). Consolidation leaves at most D(n) + 1 trees, so
            Φ falls by about t, which pays for the long root list. Amortised O(log n).
          </li>
        </ul>
      </section>

      <section className="algo-section" aria-labelledby="algo-graphs">
        <h2 id="algo-graphs">How ParcelHub uses the heaps</h2>
        <dl className="algo-uses">
          {USES.map((u) => (
            <div key={u.who}>
              <dt>{u.who}</dt>
              <dd>{u.how}</dd>
            </div>
          ))}
        </dl>
        <Complexity rows={GRAPH_ALGORITHMS} firstHeader="Algorithm" nameKey="name" />
        <p className="muted">
          V is the number of hubs and E the number of roads. The Fibonacci bound is amortised. It is better only when E is much larger than V log V,
          because that is when decrease-key dominates the work.
        </p>
      </section>

      <section className="algo-section" aria-labelledby="algo-reading">
        <h2 id="algo-reading">Reading the benchmark results</h2>
        <ul className="algo-list">
          <li>The bounds above describe growth, not speed. A heap with a better bound can still be slower on real input if each operation does more bookkeeping.</li>
          <li>Fibonacci nodes carry parent, child and sibling links plus a mark, and consolidation costs extra comparisons. On a sparse city there are few decrease-keys to save, so the overhead can outweigh the gain. Dense cities are where its bound should start to show.</li>
          <li>Merge is where the structural difference should be clearest: O(n) for Binary against O(log n) or O(1) for the others. Moving the id-to-node tables adds a cost proportional to the smaller side, which can shrink the gap.</li>
          <li>Fewer comparisons do not always mean less time. Memory layout matters too: a Binary Heap is one contiguous array, while the others follow pointers.</li>
        </ul>
        <p>
          The <a href="#/benchmarks">Benchmarks page</a> shows what was actually measured on this machine. Compare it with this page and explain any difference, rather than assuming the theory is right.
        </p>
      </section>
    </div>
  );
}
