import { HEAPS, hubLabel } from '../city.js';

export function Field({ label, children, hint }) {
  return (
    <label className="field">
      <span className="field-label">{label}</span>
      {children}
      {hint && <span className="field-hint">{hint}</span>}
    </label>
  );
}

export function HubSelect({ value, onChange, nodes, allowNone = false, noneLabel = 'No destination', id }) {
  return (
    <select id={id} value={value} onChange={(e) => onChange(Number(e.target.value))}>
      {allowNone && <option value={0}>{noneLabel}</option>}
      {Array.from({ length: nodes }, (_, i) => i + 1).map((h) => <option key={h} value={h}>{hubLabel(h)}</option>)}
    </select>
  );
}

export function HeapSelect({ value, onChange }) {
  return (
    <select value={value} onChange={(e) => onChange(e.target.value)}>
      {HEAPS.map((h) => <option key={h.id} value={h.id}>{h.label}</option>)}
      <option value="all">Compare all three</option>
    </select>
  );
}

export function Notice({ kind = 'info', title, children }) {
  return (
    <div className={`notice notice-${kind}`} role={kind === 'error' ? 'alert' : 'status'}>
      {title && <strong>{title}</strong>}
      {children && <div>{children}</div>}
    </div>
  );
}

export function Problems({ items }) {
  if (!items.length) return null;
  return (
    <Notice kind="error" title="Fix the city before calculating">
      <ul>{items.slice(0, 5).map((p) => <li key={p}>{p}</li>)}{items.length > 5 && <li>and {items.length - 5} more</li>}</ul>
    </Notice>
  );
}

const fmt = (n) => Number(n).toLocaleString('en');

// Heap operation counts. One row per heap that answered.
export function OpsTable({ results }) {
  return (
    <div className="scroll"><table className="data ops">
      <caption className="sr-only">Heap operations used by each heap</caption>
      <thead>
        <tr><th scope="col">Heap</th><th scope="col" className="num">Inserts</th><th scope="col" className="num">Extracts</th><th scope="col" className="num">Decrease-key</th><th scope="col" className="num">Compares</th></tr>
      </thead>
      <tbody>
        {results.map((r) => (
          <tr key={r.heap}>
            <th scope="row">{r.heap.replace(' Heap', '')}</th>
            <td className="num">{fmt(r.heapOps.inserts)}</td>
            <td className="num">{fmt(r.heapOps.extracts)}</td>
            <td className="num">{fmt(r.heapOps.decreaseKeys)}</td>
            <td className="num">{fmt(r.heapOps.comparisons)}</td>
          </tr>
        ))}
      </tbody>
    </table></div>
  );
}

export function Agreement({ data }) {
  if (data?.heap !== 'all') return null;
  return data.identical
    ? <p className="agree agree-yes"><span aria-hidden="true">✓</span> All three heaps gave the same answer.</p>
    : <p className="agree agree-no"><span aria-hidden="true">✕</span> The heaps disagree. This is a bug: please report it.</p>;
}

export function Busy({ loading, children }) {
  return <div className={loading ? 'is-loading' : undefined} aria-busy={loading || undefined}>{children}</div>;
}
