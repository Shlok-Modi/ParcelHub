import { useEffect, useMemo, useRef, useState } from 'react';
import { ApiError, getBenchmarkResults, runBenchmark } from '../api.js';
import { aggregate, available, BENCHMARKS, HEAP_SERIES, largestSizeSummary, parseReport, slice, spreadIsWide } from '../bench.js';
import { Notice } from '../components/ui.jsx';
import LineChart from '../charts/LineChart.jsx';
import { formatCount, formatMs, formatTick } from '../charts/scales.js';

// Series colours: the validated categorical slots 1 to 3 (see DESIGN notes in the README).
export const HEAP_COLORS = { binary: 'var(--series-1)', binomial: 'var(--series-2)', fibonacci: 'var(--series-3)' };

// "baseline" for the Binary Heap itself, otherwise how long the heap took relative to it.
export function versusBinary(row) {
  if (row.id === 'binary') return 'baseline';
  if (row.vsBinary == null) return '–';
  return `${row.vsBinary.toFixed(2)}× the time`;
}

const QUICK = { repeats: 3, warmups: 1, instances: 1, graphSizes: [100, 500, 1000], parcelSizes: [1000, 5000] };

export default function BenchmarksPage() {
  const [source, setSource] = useState({ kind: 'loading', report: null, error: null });
  const [benchmark, setBenchmark] = useState('dijkstra');
  const [density, setDensity] = useState('sparse');
  const [logY, setLogY] = useState(false);
  const [tableOpen, setTableOpen] = useState(false);
  const [running, setRunning] = useState(false);
  const [message, setMessage] = useState(null);
  const saved = useRef(null);
  const fileInput = useRef(null);

  useEffect(() => {
    const c = new AbortController();
    getBenchmarkResults({ signal: c.signal })
      .then((data) => { const report = parseReport(data); saved.current = report; setSource({ kind: 'saved', report, error: null }); })
      .catch((error) => {
        if (c.signal.aborted) return;
        setSource({ kind: 'none', report: null, error: error instanceof ApiError && error.code === 'no_results' ? null : error });
      });
    return () => c.abort();
  }, []);

  const report = source.report;
  const points = useMemo(() => (report ? aggregate(report.rows) : []), [report]);
  const { benchmarks, densities } = useMemo(() => available(points), [points]);

  // Keep the filters pointing at something the report actually contains.
  const activeBenchmark = benchmarks.some((b) => b.id === benchmark) ? benchmark : benchmarks[0]?.id;
  const meta = BENCHMARKS.find((b) => b.id === activeBenchmark);
  const dens = densities[activeBenchmark] ?? [];
  const activeDensity = meta?.graph ? (dens.includes(density) ? density : dens[0]) : '';
  const data = useMemo(() => (activeBenchmark ? slice(points, activeBenchmark, activeDensity) : { sizes: [], series: [] }), [points, activeBenchmark, activeDensity]);
  const summary = useMemo(() => largestSizeSummary(data), [data]);
  const hintLog = !logY && (spreadIsWide(data.series, (p) => p.comparisons) || spreadIsWide(data.series, (p) => p.medianMs));

  async function onFile(event) {
    const file = event.target.files?.[0];
    event.target.value = '';
    if (!file) return;
    try {
      const text = await file.text();
      setSource({ kind: 'file', report: parseReport(text), error: null, name: file.name });
      setMessage(null);
    } catch (error) {
      setMessage({ kind: 'error', title: "Couldn't open that file", text: error.message });
    }
  }
  async function quickRun() {
    setRunning(true);
    setMessage(null);
    try {
      const out = await runBenchmark(QUICK);
      setSource({ kind: 'quick', report: parseReport(out), error: null });
    } catch (error) {
      setMessage({ kind: 'error', title: "Couldn't run the benchmark", text: error.message });
    } finally {
      setRunning(false);
    }
  }
  const showSaved = () => saved.current && setSource({ kind: 'saved', report: saved.current, error: null });

  const sourceLabel = { saved: 'Saved results from the benchmark tool', file: `File: ${source.name ?? ''}`, quick: 'Quick run from this session (not saved)' }[source.kind];

  return (
    <div className="bench-page">
      <header className="bench-head">
        <h1>Benchmarks</h1>
        <p className="stage-sub">Binary, Binomial and Fibonacci heaps on the same inputs. Every result was checked against an independent solution before it was timed.</p>
      </header>

      <div className="toolbar" role="group" aria-label="Data source">
        <button type="button" className="btn" onClick={quickRun} disabled={running}>{running ? 'Running, up to a minute…' : 'Run a quick benchmark'}</button>
        <button type="button" className="btn" onClick={() => fileInput.current?.click()}>Open a results file</button>
        <input ref={fileInput} type="file" accept=".json,application/json" hidden onChange={onFile} aria-label="Benchmark results file" />
        {source.kind !== 'saved' && saved.current && <button type="button" className="btn btn-quiet" onClick={showSaved}>Back to saved results</button>}
        <a className="btn btn-quiet" href="/api/benchmark/results?format=csv" download="benchmark_results.csv">Download saved CSV</a>
      </div>

      {message && <Notice kind={message.kind} title={message.title}>{message.text}</Notice>}
      {source.kind === 'loading' && <p className="muted" role="status">Loading results…</p>}
      {source.kind === 'none' && (
        <Notice kind={source.error ? 'error' : 'info'} title={source.error ? "Couldn't load the saved results" : 'No saved results yet'}>
          {source.error
            ? source.error.message
            : <>Run <code>build\parcelhub_bench.exe --profile standard</code> in the project folder, then reload this page. Or use "Run a quick benchmark" above.</>}
        </Notice>
      )}

      {report && points.length > 0 && (
        <>
          {report.failures.length > 0 && (
            <Notice kind="error" title={`${report.failures.length} measurement${report.failures.length === 1 ? ' was' : 's were'} dropped because the answer was wrong`}>
              Their timings are not shown. The first problem: {report.failures[0].heap}, {report.failures[0].benchmark}: {report.failures[0].message}
            </Notice>
          )}
          {report.meta?.optimized === false && (
            <Notice kind="warn" title="These timings are not representative">The benchmark tool was built without optimisation. Rebuild with build.ps1 and run it again.</Notice>
          )}

          <div className="filters" role="group" aria-label="Filters">
            <fieldset className="segmented">
              <legend>Algorithm</legend>
              {benchmarks.map((b) => (
                <label key={b.id} className={b.id === activeBenchmark ? 'is-on' : undefined}>
                  <input type="radio" name="benchmark" checked={b.id === activeBenchmark} onChange={() => setBenchmark(b.id)} />
                  {b.label}
                </label>
              ))}
            </fieldset>
            {meta?.graph && dens.length > 0 && (
              <fieldset className="segmented">
                <legend>City density</legend>
                {dens.map((d) => (
                  <label key={d} className={d === activeDensity ? 'is-on' : undefined}>
                    <input type="radio" name="density" checked={d === activeDensity} onChange={() => setDensity(d)} />
                    {d[0].toUpperCase() + d.slice(1)}
                  </label>
                ))}
              </fieldset>
            )}
            <label className="check"><input type="checkbox" checked={logY} onChange={(e) => setLogY(e.target.checked)} /> Log scale for the vertical axis</label>
            {hintLog && <span className="hint" role="note">Some values are far smaller than others. The log scale makes them visible.</span>}
          </div>

          <p className="bench-context">
            <strong>{meta?.long}</strong>{activeDensity ? `, ${activeDensity} city` : ''}. {sourceLabel}.
            {report.meta?.repeats ? ` Median of ${report.meta.repeats} runs, averaged over ${report.meta.instances} input${report.meta.instances === 1 ? '' : 's'}.` : ''}
          </p>

          <div className="charts">
            <LineChart
              title="Time" subtitle="median, lower is faster"
              xValues={data.sizes} xLabel={meta ? meta.unit[0].toUpperCase() + meta.unit.slice(1) : ''} yLabel="Milliseconds" logY={logY}
              formatY={formatMs} formatYTick={formatTick}
              series={data.series.map((s) => ({ id: s.id, name: s.name, marker: s.marker, color: HEAP_COLORS[s.id], points: s.points.map((p) => ({ x: p.size, y: p.medianMs })) }))}
            />
            <LineChart
              title="Comparisons" subtitle="key comparisons inside the heap"
              xValues={data.sizes} xLabel={meta ? meta.unit[0].toUpperCase() + meta.unit.slice(1) : ''} yLabel="Comparisons" logY={logY}
              formatY={formatCount} formatYTick={formatTick}
              series={data.series.map((s) => ({ id: s.id, name: s.name, marker: s.marker, color: HEAP_COLORS[s.id], points: s.points.map((p) => ({ x: p.size, y: p.comparisons })) }))}
            />
          </div>

          {summary && (
            <section className="summary" aria-labelledby="summary-title">
              <h2 id="summary-title">At {formatCount(summary.size)} {meta?.unit}</h2>
              <div className="scroll"><table className="data">
                <caption className="sr-only">Heaps compared at the largest size</caption>
                <thead><tr><th scope="col">Heap</th><th scope="col" className="num">Median time</th><th scope="col" className="num">Time compared with Binary</th><th scope="col" className="num">Comparisons</th></tr></thead>
                <tbody>
                  {summary.rows.map((r) => (
                    <tr key={r.id} className={r.id === summary.fastest ? 'row-hot' : undefined}>
                      <th scope="row"><span className="swatch" style={{ background: HEAP_COLORS[r.id] }} aria-hidden="true" />{r.name}{r.id === summary.fastest && <span className="fastest"> fastest</span>}</th>
                      <td className="num">{formatMs(r.medianMs)}</td>
                      <td className="num">{versusBinary(r)}</td>
                      <td className="num">{formatCount(r.comparisons)}</td>
                    </tr>
                  ))}
                </tbody>
              </table></div>
              <p className="muted">Fewer comparisons do not always mean less time: the heaps also differ in memory use and bookkeeping. Report both.</p>
            </section>
          )}

          <details className="fold" open={tableOpen} onToggle={(e) => setTableOpen(e.currentTarget.open)}>
            <summary><h2>Show all values as a table</h2></summary>
            <div className="scroll">
              <table className="data">
                <caption className="sr-only">Median time and comparisons for every heap and size</caption>
                <thead><tr><th scope="col">Size</th>{data.series.map((s) => <th scope="col" key={s.id} className="num">{s.name} time</th>)}{data.series.map((s) => <th scope="col" key={`c${s.id}`} className="num">{s.name} comparisons</th>)}</tr></thead>
                <tbody>
                  {data.sizes.map((size) => (
                    <tr key={size}>
                      <th scope="row">{formatCount(size)}</th>
                      {data.series.map((s) => <td key={s.id} className="num">{formatMs(s.points.find((p) => p.size === size)?.medianMs)}</td>)}
                      {data.series.map((s) => { const p = s.points.find((q) => q.size === size); return <td key={`c${s.id}`} className="num">{p ? formatCount(p.comparisons) : '–'}</td>; })}
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </details>

          <p className="muted bench-meta">
            {report.meta?.compiler && <>Built with {report.meta.compiler}. </>}
            {report.meta?.seed != null && <>Seed {report.meta.seed}: the same seed reproduces the same inputs. </>}
            {HEAP_SERIES.length} heaps, {report.rows.length} measured series.
          </p>
        </>
      )}
    </div>
  );
}
