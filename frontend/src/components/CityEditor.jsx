import { useState } from 'react';
import { cityProblems, demoCity, LIMITS, pruneCity } from '../city.js';
import { layoutGraph } from '../layout.js';
import { Field } from './ui.jsx';

const numberOrNaN = (text) => (text === '' ? NaN : Number(text));
const show = (v) => (Number.isFinite(v) ? v : '');

// Edits the shared city. Every change goes through setCity, which keeps the map layout stable.
export default function CityEditor({ city, setCity }) {
  const [draft, setDraft] = useState({ from: 1, to: 2, weight: 5 });
  const [error, setError] = useState('');

  const update = (next) => setCity((c) => {
    const merged = pruneCity({ ...c, ...next });
    return { ...merged, positions: layoutGraph(merged.nodes, merged.edges, merged.positions) };
  });

  const setHubs = (text) => {
    const n = Math.round(Number(text));
    if (!Number.isFinite(n)) return;
    update({ nodes: Math.min(LIMITS.maxHubs, Math.max(1, n)) });
  };
  const setRoad = (i, patch) => update({ edges: city.edges.map((e, k) => (k === i ? { ...e, ...patch } : e)) });
  const removeRoad = (i) => update({ edges: city.edges.filter((_, k) => k !== i) });

  function addRoad() {
    const road = { from: draft.from, to: draft.to, weight: draft.weight };
    const problems = cityProblems({ ...city, edges: [road] }).filter((p) => p.startsWith('Road 1'));
    if (city.edges.length >= LIMITS.maxRoads) { setError(`The city can have at most ${LIMITS.maxRoads} roads.`); return; }
    if (problems.length) { setError(problems[0].replace('Road 1', 'This road')); return; }
    setError('');
    update({ edges: [...city.edges, road] });
  }

  return (
    <div className="editor">
      <div className="editor-top">
        <Field label="Hubs" hint={`1 to ${LIMITS.maxHubs}`}>
          <input type="number" min={1} max={LIMITS.maxHubs} value={city.nodes} onChange={(e) => setHubs(e.target.value)} />
        </Field>
        <button type="button" className="btn btn-quiet" onClick={() => setCity(demoCity())}>Reset to demo city</button>
      </div>

      <table className="data editor-table">
        <caption className="sr-only">Roads</caption>
        <thead><tr><th scope="col">From</th><th scope="col">To</th><th scope="col">Cost</th><th scope="col"><span className="sr-only">Remove</span></th></tr></thead>
        <tbody>
          {city.edges.map((e, i) => (
            <tr key={i}>
              <td><input aria-label={`Road ${i + 1} from`} type="number" min={1} max={city.nodes} value={show(e.from)} onChange={(ev) => setRoad(i, { from: numberOrNaN(ev.target.value) })} /></td>
              <td><input aria-label={`Road ${i + 1} to`} type="number" min={1} max={city.nodes} value={show(e.to)} onChange={(ev) => setRoad(i, { to: numberOrNaN(ev.target.value) })} /></td>
              <td><input aria-label={`Road ${i + 1} cost`} type="number" min={0} max={LIMITS.maxWeight} value={show(e.weight)} onChange={(ev) => setRoad(i, { weight: numberOrNaN(ev.target.value) })} /></td>
              <td><button type="button" className="btn btn-icon" aria-label={`Remove road ${i + 1}`} onClick={() => removeRoad(i)}>Remove</button></td>
            </tr>
          ))}
          {city.edges.length === 0 && <tr><td colSpan={4} className="empty-row">No roads yet. Add one below.</td></tr>}
        </tbody>
      </table>

      <fieldset className="add-road">
        <legend>Add a road</legend>
        <Field label="From"><input type="number" min={1} max={city.nodes} value={show(draft.from)} onChange={(e) => setDraft({ ...draft, from: numberOrNaN(e.target.value) })} /></Field>
        <Field label="To"><input type="number" min={1} max={city.nodes} value={show(draft.to)} onChange={(e) => setDraft({ ...draft, to: numberOrNaN(e.target.value) })} /></Field>
        <Field label="Cost"><input type="number" min={0} max={LIMITS.maxWeight} value={show(draft.weight)} onChange={(e) => setDraft({ ...draft, weight: numberOrNaN(e.target.value) })} /></Field>
        <button type="button" className="btn" onClick={addRoad}>Add road</button>
      </fieldset>
      {error && <p className="field-error" role="alert">{error}</p>}
    </div>
  );
}
