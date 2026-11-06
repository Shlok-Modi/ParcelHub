import { useEffect, useMemo, useRef, useState } from 'react';
import { postMerge } from '../api.js';
import { cityProblems, hubLabel, hubOfParcel, LEVELS, levelLabel, mergeRequest } from '../city.js';
import { pathText } from '../pages/RoutesPage.jsx';
import { Agreement, Busy, Field, HeapSelect, Notice, OpsTable } from './ui.jsx';

const CHECKS = [
  ['countsMatch', 'The merged queue holds every parcel from both hubs'],
  ['hubBEmpty', 'Hub B is empty after the merge'],
  ['noParcelLost', 'No parcel was lost'],
  ['noDuplicates', 'No parcel appears twice'],
  ['orderMatchesSortedKeys', 'The dispatch order matches the parcels sorted by priority, then arrival, then id'],
];

// The server reports levels as display names ("Same-day"); the tags are keyed by id.
const levelKey = (name) => LEVELS.find((l) => l.label === name)?.id ?? 'standard';

export default function MergePanel({ city }) {
  const [heap, setHeap] = useState('all');
  const [overrides, setOverrides] = useState({});
  const [run, setRun] = useState({ status: 'idle', data: null, error: null });
  const controller = useRef(null);

  const problems = useMemo(() => cityProblems(city), [city]);
  const inputKey = JSON.stringify([city.nodes, city.edges, city.parcels, city.upgrades, overrides, heap]);

  // Changing the city, the hub split or the heap makes an earlier merge out of date.
  useEffect(() => {
    controller.current?.abort();
    setRun({ status: 'idle', data: null, error: null });
  }, [inputKey]);

  const upgradedLevel = (p) => city.upgrades.find((u) => u.id === p.id)?.level ?? p.level;
  const sizes = city.parcels.reduce((n, p, i) => { n[hubOfParcel(overrides, p, i)] += 1; return n; }, { A: 0, B: 0 });

  async function merge() {
    controller.current?.abort();
    const c = new AbortController();
    controller.current = c;
    setRun({ status: 'loading', data: null, error: null });
    try {
      const data = await postMerge(mergeRequest(city, { heap, overrides }), { signal: c.signal });
      if (!c.signal.aborted) setRun({ status: 'ready', data, error: null });
    } catch (error) {
      if (!c.signal.aborted) setRun({ status: 'error', data: null, error });
    }
  }

  const results = run.data ? (run.data.heap === 'all' ? run.data.results : [run.data]) : [];
  const first = results[0];
  const verified = results.length > 0 && results.every((r) => r.verified);

  return (
    <section className="parcels merge-panel" aria-labelledby="merge-title">
      <h2 id="merge-title">Merge two hubs</h2>
      <p className="muted">
        Split the parcels between Hub A and Hub B, then merge the two queues into one. Upgrades are applied first.
        The check below confirms that no parcel is lost or duplicated and that the combined queue dispatches in the right order.
      </p>

      <div className="scroll scroll-tall">
        <table className="data compact">
          <caption className="sr-only">Which hub each parcel starts in</caption>
          <thead><tr><th scope="col">Parcel</th><th scope="col">Service</th><th scope="col">Starts in</th></tr></thead>
          <tbody>
            {city.parcels.map((p, i) => (
              <tr key={p.id}>
                <th scope="row">{p.id}</th>
                <td><span className={`tag tag-${upgradedLevel(p)}`}>{levelLabel(upgradedLevel(p))}</span></td>
                <td>
                  <select
                    className="hub-pick" value={hubOfParcel(overrides, p, i)} aria-label={`Hub for parcel ${p.id}`}
                    onChange={(e) => setOverrides((o) => ({ ...o, [p.id]: e.target.value }))}
                  >
                    <option value="A">Hub A</option>
                    <option value="B">Hub B</option>
                  </select>
                </td>
              </tr>
            ))}
            {city.parcels.length === 0 && <tr><td colSpan={3} className="empty-row">No parcels to merge. Add some above.</td></tr>}
          </tbody>
        </table>
      </div>

      <div className="row row-tight">
        <Field label="Heap"><HeapSelect value={heap} onChange={setHeap} /></Field>
        <button type="button" className="btn btn-primary" onClick={merge} disabled={problems.length > 0 || city.parcels.length === 0 || run.status === 'loading'}>
          {run.status === 'loading' ? 'Merging…' : 'Merge and dispatch'}
        </button>
      </div>
      <p className="muted">Hub A: {sizes.A} {sizes.A === 1 ? 'parcel' : 'parcels'}. Hub B: {sizes.B} {sizes.B === 1 ? 'parcel' : 'parcels'}.</p>
      {problems.length > 0 && <Notice kind="error" title="Fix the city before merging">{problems[0]}</Notice>}
      {run.error && <Notice kind="error" title="Merge failed">{run.error.message}</Notice>}

      <Busy loading={run.status === 'loading'}>
        {run.status === 'ready' && first && (
          <div className="merge-result" role="region" aria-label="Merge result">
            <h3>Result</h3>
            <p className="answer-none" role="status">
              Hub A ({first.hubA.parcels}) and Hub B ({first.hubB.parcels}) merged into one queue of {first.merged}.
              {' '}{verified ? 'Every check passed.' : 'A check failed. This is a bug: please report it.'}
            </p>
            <ul className="checks" aria-label="Merge checks">
              {CHECKS.map(([key, text]) => (
                <li key={key} className={results.every((r) => r.checks[key]) ? undefined : 'is-bad'}>{text}</li>
              ))}
            </ul>
            <Agreement data={run.data} />

            <h3>Dispatch order after the merge</h3>
            {first.dispatches.length === 0 ? <p className="muted">There were no parcels to dispatch.</p> : (
              <ol className="manifest">
                {first.dispatches.map((d, i) => (
                  <li key={d.id}>
                    <span className="seq">{i + 1}</span>
                    <span className="manifest-main">
                      <strong>Parcel {d.id}</strong> <span className={`tag tag-${levelKey(d.level)}`}>{d.level}</span>{' '}
                      <span className={`origin origin-${d.origin.toLowerCase()}`}>from Hub {d.origin}</span>
                    </span>
                    <span className="manifest-sub">
                      {d.reachable ? <>{pathText(d.path)}, cost {d.cost}</> : <>No route: {hubLabel(d.destination)} can't be reached from {hubLabel(d.source)}</>}
                    </span>
                  </li>
                ))}
              </ol>
            )}

            <h3>Heap work (insert, extract and one merge)</h3>
            <OpsTable results={results} />
            <p className="muted">Merges: {first.heapOps.merges}. Binary Heap rebuilds on merge, while Binomial and Fibonacci heaps join their trees or root lists.</p>
          </div>
        )}
      </Busy>
    </section>
  );
}
