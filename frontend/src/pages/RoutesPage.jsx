import { useMemo, useState } from 'react';
import { postMst, postRoute } from '../api.js';
import CityEditor from '../components/CityEditor.jsx';
import GraphBoard from '../components/GraphBoard.jsx';
import { Agreement, Busy, Field, HeapSelect, HubSelect, Notice, OpsTable, Problems } from '../components/ui.jsx';
import { cityProblems, hubLabel, mstRequest, routeRequest } from '../city.js';
import { useRequest } from '../hooks.js';

export const pathText = (path) => path.map(hubLabel).join(' → ');

export default function RoutesPage({ city, setCity }) {
  const [heap, setHeap] = useState('all');
  const [source, setSource] = useState(1);
  const [destination, setDestination] = useState(7);
  const [showTree, setShowTree] = useState(false);
  const [treeStart, setTreeStart] = useState(1);

  const src = Math.min(source, city.nodes);
  const dst = destination > city.nodes ? 0 : destination;
  const start = Math.min(treeStart, city.nodes);
  const problems = useMemo(() => cityProblems(city), [city]);
  const ready = problems.length === 0;
  const cityKey = JSON.stringify([city.nodes, city.edges]);

  const route = useRequest((signal) => postRoute(routeRequest(city, { heap, source: src, destination: dst }), { signal }),
    [cityKey, heap, src, dst], { delay: 120, enabled: ready });
  const tree = useRequest((signal) => postMst(mstRequest(city, { heap, start }), { signal }),
    [cityKey, heap, start], { delay: 120, enabled: ready && showTree });

  const first = route.data && (route.data.heap === 'all' ? route.data.results[0] : route.data);
  const results = route.data ? (route.data.heap === 'all' ? route.data.results : [route.data]) : [];
  const treeFirst = tree.data && (tree.data.heap === 'all' ? tree.data.results[0] : tree.data);
  const treeResults = tree.data ? (tree.data.heap === 'all' ? tree.data.results : [tree.data]) : [];

  const distances = first?.distances ?? null;
  const unreachable = useMemo(() => {
    if (!distances) return null;
    return new Set(distances.map((d, i) => (d === null ? i + 1 : 0)).filter(Boolean));
  }, [distances]);
  const badges = distances && city.nodes <= 40 ? Object.fromEntries(distances.map((d, i) => [i + 1, d === null ? '–' : d])) : null;

  const move = (hub, p) => setCity((c) => ({ ...c, positions: { ...c.positions, [hub]: p } }));
  const pick = (hub) => { if (hub === src) return; setDestination(hub); };

  return (
    <div className="layout">
      <section className="stage" aria-labelledby="map-title">
        <div className="stage-head">
          <h1 id="map-title">City map</h1>
          <p className="stage-sub">Click a hub to set the destination. Drag hubs to rearrange them.</p>
        </div>
        <Busy loading={route.loading || tree.loading}>
          <GraphBoard
            nodes={city.nodes} edges={city.edges} positions={city.positions}
            path={first?.reachable ? first.path : []}
            tree={showTree && treeFirst ? treeFirst.edges : []}
            source={src} destination={dst} unreachable={unreachable} badges={badges}
            onMove={move} onPick={pick}
          />
        </Busy>
        <ul className="key" aria-label="Map key">
          <li><span className="key-swatch key-road" aria-hidden="true" />Road, with its cost</li>
          <li><span className="key-swatch key-path" aria-hidden="true" />Chosen route</li>
          {showTree && <li><span className="key-swatch key-tree" aria-hidden="true" />Minimum spanning tree</li>}
          {showTree && first?.reachable && <li><span className="key-swatch key-both" aria-hidden="true" />Road in both</li>}
          {badges && <li><span className="key-number" aria-hidden="true">12</span>Distance from {hubLabel(src)}</li>}
        </ul>
      </section>

      <aside className="inspector" aria-label="Route details">
        <Problems items={problems} />

        <section className="block" aria-labelledby="route-title">
          <h2 id="route-title">Shortest route</h2>
          <div className="row">
            <Field label="From"><HubSelect value={src} nodes={city.nodes} onChange={setSource} /></Field>
            <Field label="To"><HubSelect value={dst} nodes={city.nodes} onChange={setDestination} allowNone noneLabel="None" /></Field>
            <Field label="Heap"><HeapSelect value={heap} onChange={setHeap} /></Field>
          </div>
          {route.error && <Notice kind="error" title="Could not calculate the route">{route.error.message}</Notice>}
          {first && dst !== 0 && (
            first.reachable ? (
              <div className="answer">
                <p className="answer-route">{pathText(first.path)}</p>
                <p className="answer-cost"><span className="big">{first.cost}</span> total travel cost</p>
              </div>
            ) : (
              <p className="answer-none">No route: {hubLabel(dst)} can't be reached from {hubLabel(src)}. There is no chain of roads between them.</p>
            )
          )}
          {first && dst === 0 && <p className="muted">Choose a destination to see its route. Distances from {hubLabel(src)} are shown on the map.</p>}
        </section>

        {first && (
          <section className="block" aria-labelledby="dist-title">
            <h2 id="dist-title">Distance from {hubLabel(src)}</h2>
            <div className="scroll">
              <table className="data compact">
                <caption className="sr-only">Shortest distance from {hubLabel(src)} to every hub</caption>
                <thead><tr><th scope="col">Hub</th><th scope="col" className="num">Distance</th></tr></thead>
                <tbody>
                  {first.distances.map((d, i) => (
                    <tr key={i} className={dst === i + 1 ? 'row-hot' : undefined}>
                      <th scope="row"><button type="button" className="linklike" onClick={() => i + 1 !== src && setDestination(i + 1)}>{hubLabel(i + 1)}</button></th>
                      <td className="num">{d === null ? 'Unreachable' : d}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </section>
        )}

        <section className="block" aria-labelledby="tree-title">
          <h2 id="tree-title">Minimum spanning tree</h2>
          <p className="muted">The cheapest set of roads that keeps every hub connected (Prim's algorithm).</p>
          <div className="row row-tight">
            <label className="check"><input type="checkbox" checked={showTree} onChange={(e) => setShowTree(e.target.checked)} /> Show on map</label>
            {showTree && <Field label="Start at"><HubSelect value={start} nodes={city.nodes} onChange={setTreeStart} /></Field>}
          </div>
          {showTree && tree.error && <Notice kind="error" title="Could not calculate the tree">{tree.error.message}</Notice>}
          {showTree && treeFirst && (
            <div className="answer">
              <p className="answer-cost"><span className="big">{treeFirst.totalWeight}</span> total cost, {treeFirst.edges.length} road{treeFirst.edges.length === 1 ? '' : 's'}</p>
              {treeFirst.isSpanningTree
                ? <p className="muted">One tree connects all {city.nodes} hubs.</p>
                : <p className="note-forest">The city is split into {treeFirst.components} separate parts, so this is a spanning forest ({treeFirst.components} trees), not a single tree.</p>}
            </div>
          )}
        </section>

        {results.length > 0 && (
          <section className="block" aria-labelledby="ops-title">
            <h2 id="ops-title">Heap work for the route</h2>
            <Agreement data={route.data} />
            <OpsTable results={results} />
          </section>
        )}
        {showTree && treeResults.length > 0 && (
          <section className="block" aria-labelledby="ops2-title">
            <h2 id="ops2-title">Heap work for the tree</h2>
            <Agreement data={tree.data} />
            <OpsTable results={treeResults} />
          </section>
        )}

        <details className="block fold">
          <summary><h2>Edit the city</h2></summary>
          <CityEditor city={city} setCity={setCity} />
        </details>
      </aside>
    </div>
  );
}
