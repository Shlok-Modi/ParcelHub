import { useEffect, useMemo, useRef, useState } from 'react';
import { postDispatch } from '../api.js';
import GraphBoard from '../components/GraphBoard.jsx';
import { Agreement, Busy, Field, HeapSelect, HubSelect, Notice, OpsTable, Problems } from '../components/ui.jsx';
import { cityProblems, dispatchRequest, hubLabel, LEVELS, levelLabel, levelRank, LIMITS } from '../city.js';
import { pathText } from './RoutesPage.jsx';

const nextId = (parcels) => parcels.reduce((m, p) => Math.max(m, p.id), 100) + 1;
const nextArrival = (parcels) => parcels.reduce((m, p) => Math.max(m, p.arrival ?? 0), -1) + 1;

function LevelTag({ level }) {
  return <span className={`tag tag-${level}`}>{levelLabel(level)}</span>;
}

export default function DispatchPage({ city, setCity }) {
  const [heap, setHeap] = useState('all');
  const [run, setRun] = useState({ status: 'idle', data: null, error: null });
  const [step, setStep] = useState(0);
  const [selected, setSelected] = useState(-1);
  const controller = useRef(null);

  const problems = useMemo(() => cityProblems(city), [city]);
  const inputKey = JSON.stringify([city.nodes, city.edges, city.parcels, city.upgrades]);

  // Changing the city or the parcels makes an earlier run out of date.
  useEffect(() => {
    controller.current?.abort();
    setRun({ status: 'idle', data: null, error: null });
    setStep(0);
    setSelected(-1);
  }, [inputKey, heap]);

  const first = run.data && (run.data.heap === 'all' ? run.data.results[0] : run.data);
  const dispatches = first?.dispatches ?? [];
  const results = run.data ? (run.data.heap === 'all' ? run.data.results : [run.data]) : [];
  const done = dispatches.slice(0, step);
  const waiting = dispatches.slice(step);
  const current = selected >= 0 && selected < done.length ? done[selected] : null;

  async function start() {
    controller.current?.abort();
    const c = new AbortController();
    controller.current = c;
    setRun({ status: 'loading', data: null, error: null });
    setStep(0);
    setSelected(-1);
    try {
      const data = await postDispatch(dispatchRequest(city, { heap }), { signal: c.signal });
      if (!c.signal.aborted) setRun({ status: 'ready', data, error: null });
    } catch (error) {
      if (!c.signal.aborted) setRun({ status: 'error', data: null, error });
    }
  }
  const dispatchNext = () => { if (step < dispatches.length) { setStep(step + 1); setSelected(step); } };
  const dispatchAll = () => { setStep(dispatches.length); setSelected(dispatches.length - 1); };
  const startOver = () => { setStep(0); setSelected(-1); };

  const unroutable = done.filter((d) => !d.reachable).length;

  return (
    <div className="layout">
      <section className="stage" aria-labelledby="dispatch-map-title">
        <div className="stage-head">
          <h1 id="dispatch-map-title">Dispatch board</h1>
          <p className="stage-sub">
            {current
              ? <>Parcel {current.id}: {hubLabel(current.source)} to {hubLabel(current.destination)}</>
              : 'Run the dispatch, then send parcels out one at a time. Each route appears here.'}
          </p>
        </div>
        <Busy loading={run.status === 'loading'}>
          <GraphBoard
            nodes={city.nodes} edges={city.edges} positions={city.positions}
            path={current?.reachable ? current.path : []}
            source={current?.source ?? 0} destination={current?.destination ?? 0}
            label="City map showing the selected parcel's route"
          />
        </Busy>
        <ParcelEditor city={city} setCity={setCity} />
      </section>

      <aside className="inspector" aria-label="Dispatch queue">
        <Problems items={problems} />
        <section className="block" aria-labelledby="run-title">
          <h2 id="run-title">Dispatch queue</h2>
          <p className="muted">Parcels leave in priority order: Express first, then Same-day, then Standard. Within a level, the earlier arrival goes first.</p>
          <div className="row row-tight">
            <Field label="Heap"><HeapSelect value={heap} onChange={setHeap} /></Field>
            <button type="button" className="btn btn-primary" onClick={start} disabled={problems.length > 0 || city.parcels.length === 0 || run.status === 'loading'}>
              {run.status === 'loading' ? 'Preparing…' : run.status === 'ready' ? 'Run again' : 'Run dispatch'}
            </button>
          </div>
          {city.parcels.length === 0 && <p className="muted">Add at least one parcel to dispatch.</p>}
          {run.error && <Notice kind="error" title="Dispatch failed">{run.error.message}</Notice>}
        </section>

        {run.status === 'ready' && (
          <>
            <section className="block" aria-labelledby="progress-title">
              <h2 id="progress-title">Progress</h2>
              <div className="stats" role="group" aria-label="Queue counts">
                <div><span className="stat-value">{waiting.length}</span><span className="stat-label">waiting</span></div>
                <div><span className="stat-value">{done.length}</span><span className="stat-label">dispatched</span></div>
              </div>
              <div className="row row-tight">
                <button type="button" className="btn btn-primary" onClick={dispatchNext} disabled={waiting.length === 0}>Dispatch next</button>
                <button type="button" className="btn" onClick={dispatchAll} disabled={waiting.length === 0}>Dispatch all</button>
                <button type="button" className="btn btn-quiet" onClick={startOver} disabled={step === 0}>Start over</button>
              </div>
              {waiting.length === 0 && dispatches.length > 0 && (
                <p className="answer-none" role="status">
                  All {dispatches.length} parcels dispatched.{unroutable > 0 && ` ${unroutable} could not be routed because the destination can't be reached.`}
                </p>
              )}
            </section>

            <section className="block" aria-labelledby="waiting-title">
              <h2 id="waiting-title">Waiting, next first</h2>
              {waiting.length === 0 ? <p className="muted">The queue is empty.</p> : (
                <ol className="manifest">
                  {waiting.map((d, i) => (
                    <li key={d.id} className={i === 0 ? 'is-next' : undefined}>
                      <span className="seq">{step + i + 1}</span>
                      <span className="manifest-main"><strong>Parcel {d.id}</strong> <LevelTag level={levelKey(d.level)} /></span>
                      <span className="manifest-sub">{hubLabel(d.source)} to {hubLabel(d.destination)}, due {d.deadline}</span>
                    </li>
                  ))}
                </ol>
              )}
            </section>

            <section className="block" aria-labelledby="sent-title">
              <h2 id="sent-title">Dispatched</h2>
              {done.length === 0 ? <p className="muted">Nothing sent yet.</p> : (
                <ol className="manifest manifest-sent">
                  {done.map((d, i) => (
                    <li key={d.id} className={i === selected ? 'is-selected' : undefined}>
                      <button type="button" className="manifest-row" onClick={() => setSelected(i)} aria-pressed={i === selected}>
                        <span className="seq">{i + 1}</span>
                        <span className="manifest-main"><strong>Parcel {d.id}</strong> <LevelTag level={levelKey(d.level)} /></span>
                        <span className="manifest-sub">
                          {d.reachable ? <>{pathText(d.path)}, cost {d.cost}</> : <>No route: {hubLabel(d.destination)} can't be reached from {hubLabel(d.source)}</>}
                        </span>
                      </button>
                    </li>
                  ))}
                </ol>
              )}
            </section>

            <section className="block" aria-labelledby="dops-title">
              <h2 id="dops-title">Heap work for the queue</h2>
              <Agreement data={run.data} />
              <OpsTable results={results} />
            </section>
          </>
        )}
      </aside>
    </div>
  );
}

// The server reports levels as display names ("Same-day"); the page keys them by id.
const levelKey = (name) => LEVELS.find((l) => l.label === name)?.id ?? 'standard';

function ParcelEditor({ city, setCity }) {
  const [draft, setDraft] = useState({ source: 1, destination: 2, level: 'standard', deadline: '17:00' });
  const [upgrade, setUpgrade] = useState({ id: 0, level: '' });
  const [error, setError] = useState('');

  const upgraded = new Set(city.upgrades.map((u) => u.id));
  const upgradable = city.parcels.filter((p) => p.level !== 'express' && !upgraded.has(p.id));
  const chosen = city.parcels.find((p) => p.id === upgrade.id) ?? upgradable[0];
  const targets = chosen ? LEVELS.filter((l) => l.rank < levelRank(chosen.level)) : [];
  const targetLevel = targets.some((l) => l.id === upgrade.level) ? upgrade.level : targets[0]?.id ?? '';

  const src = Math.min(draft.source, city.nodes);
  const dst = Math.min(draft.destination, city.nodes);

  function addParcel() {
    if (city.parcels.length >= LIMITS.maxParcels) { setError(`You can have at most ${LIMITS.maxParcels} parcels.`); return; }
    if (!/^([01]\d|2[0-3]):[0-5]\d$/.test(draft.deadline)) { setError('Enter the deadline as a time, for example 17:30.'); return; }
    setError('');
    setCity((c) => ({
      ...c,
      parcels: [...c.parcels, { id: nextId(c.parcels), source: src, destination: dst, level: draft.level, deadline: draft.deadline, arrival: nextArrival(c.parcels) }],
    }));
  }
  const removeParcel = (id) => setCity((c) => ({ ...c, parcels: c.parcels.filter((p) => p.id !== id), upgrades: c.upgrades.filter((u) => u.id !== id) }));
  const addUpgrade = () => {
    if (!chosen || !targetLevel) return;
    setCity((c) => ({ ...c, upgrades: [...c.upgrades, { id: chosen.id, level: targetLevel }] }));
    setUpgrade({ id: 0, level: '' });
  };
  const removeUpgrade = (id) => setCity((c) => ({ ...c, upgrades: c.upgrades.filter((u) => u.id !== id) }));

  return (
    <div className="parcels">
      <h2>Parcels</h2>
      <div className="scroll scroll-tall">
        <table className="data">
          <caption className="sr-only">Parcels waiting to be dispatched</caption>
          <thead>
            <tr><th scope="col">Parcel</th><th scope="col">From</th><th scope="col">To</th><th scope="col">Service</th><th scope="col">Due</th><th scope="col">Arrival</th><th scope="col"><span className="sr-only">Remove</span></th></tr>
          </thead>
          <tbody>
            {city.parcels.map((p) => {
              const up = city.upgrades.find((u) => u.id === p.id);
              return (
                <tr key={p.id}>
                  <th scope="row">{p.id}</th>
                  <td>{hubLabel(p.source)}</td>
                  <td>{hubLabel(p.destination)}</td>
                  <td>
                    <LevelTag level={p.level} />
                    {up && <span className="upgrade-note"> upgraded to <LevelTag level={up.level} /> <button type="button" className="linklike" onClick={() => removeUpgrade(p.id)} aria-label={`Undo upgrade of parcel ${p.id}`}>undo</button></span>}
                  </td>
                  <td>{p.deadline}</td>
                  <td className="num">{p.arrival}</td>
                  <td><button type="button" className="btn btn-icon" onClick={() => removeParcel(p.id)} aria-label={`Remove parcel ${p.id}`}>Remove</button></td>
                </tr>
              );
            })}
            {city.parcels.length === 0 && <tr><td colSpan={7} className="empty-row">No parcels. Add one below.</td></tr>}
          </tbody>
        </table>
      </div>

      <div className="forms">
        <fieldset className="add-parcel">
          <legend>Add a parcel</legend>
          <Field label="From"><HubSelect value={src} nodes={city.nodes} onChange={(v) => setDraft({ ...draft, source: v })} /></Field>
          <Field label="To"><HubSelect value={dst} nodes={city.nodes} onChange={(v) => setDraft({ ...draft, destination: v })} /></Field>
          <Field label="Service">
            <select value={draft.level} onChange={(e) => setDraft({ ...draft, level: e.target.value })}>
              {LEVELS.map((l) => <option key={l.id} value={l.id}>{l.label}</option>)}
            </select>
          </Field>
          <Field label="Due"><input type="time" value={draft.deadline} onChange={(e) => setDraft({ ...draft, deadline: e.target.value })} /></Field>
          <button type="button" className="btn" onClick={addParcel}>Add parcel</button>
        </fieldset>

        <fieldset className="add-parcel">
          <legend>Upgrade a parcel</legend>
          {upgradable.length === 0 ? <p className="muted">Every parcel is already Express or upgraded.</p> : (
            <>
              <Field label="Parcel">
                <select value={chosen?.id ?? ''} onChange={(e) => setUpgrade({ id: Number(e.target.value), level: '' })}>
                  {upgradable.map((p) => <option key={p.id} value={p.id}>{p.id} ({levelLabel(p.level)})</option>)}
                </select>
              </Field>
              <Field label="Upgrade to">
                <select value={targetLevel} onChange={(e) => setUpgrade({ id: chosen.id, level: e.target.value })}>
                  {targets.map((l) => <option key={l.id} value={l.id}>{l.label}</option>)}
                </select>
              </Field>
              <button type="button" className="btn" onClick={addUpgrade}>Upgrade</button>
            </>
          )}
        </fieldset>
      </div>
      {error && <p className="field-error" role="alert">{error}</p>}
    </div>
  );
}
