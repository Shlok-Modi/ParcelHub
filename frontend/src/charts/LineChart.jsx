import { useId, useRef, useState } from 'react';
import { clamp01, linearPos, linearTicks, logPos, logTicks, compact } from './scales.js';

const W = 600, H = 330;
const M = { left: 62, right: 26, top: 14, bottom: 46 };
const PW = W - M.left - M.right, PH = H - M.top - M.bottom;

function Marker({ shape, x, y, color }) {
  const common = { fill: color, className: 'mark' };
  if (shape === 'square') return <rect {...common} x={x - 4.5} y={y - 4.5} width={9} height={9} rx={1.5} />;
  if (shape === 'triangle') return <path {...common} d={`M${x} ${y - 6} L${x + 5.6} ${y + 4} L${x - 5.6} ${y + 4} Z`} strokeLinejoin="round" />;
  return <circle {...common} cx={x} cy={y} r={5} />;
}

// A line chart with a log-spaced x axis (workload sizes) and a linear or log y axis.
// series: [{ id, name, color, marker, points: [{ x, y }] }]. Hover or arrow keys read exact values.
export default function LineChart({ title, subtitle, series, xValues, xLabel, yLabel, formatY, formatYTick, formatX = compact, logY = false }) {
  const uid = useId();
  const [active, setActive] = useState(-1);
  const wrap = useRef(null);

  const xs = xValues;
  const xMin = xs[0], xMax = xs[xs.length - 1];
  const xPos = (v) => (xs.length === 1 ? 0.5 : logPos(v, xMin, xMax));
  const px = (v) => M.left + xPos(v) * PW;

  const ys = series.flatMap((s) => s.points.map((p) => p.y)).filter((v) => Number.isFinite(v));
  const yMaxData = Math.max(...ys, 0), yMinData = Math.min(...ys.filter((v) => v > 0), yMaxData || 1);
  const lin = linearTicks(yMaxData);
  const log = logTicks(yMinData / 1.5, yMaxData * 1.5 || 1);
  const yTicks = logY ? log.ticks : lin.ticks;
  const yLo = logY ? log.min : 0, yHi = logY ? log.max : lin.max;
  const py = (v) => M.top + (1 - clamp01(logY ? logPos(v, yLo, yHi) : linearPos(v, yLo, yHi))) * PH;

  const move = (event) => {
    const box = event.currentTarget.getBoundingClientRect();
    const x = ((event.clientX - box.left) / box.width) * W;
    let best = 0, dist = Infinity;
    xs.forEach((v, i) => { const d = Math.abs(px(v) - x); if (d < dist) { dist = d; best = i; } });
    setActive(best);
  };
  const keys = (event) => {
    if (event.key === 'ArrowRight') { event.preventDefault(); setActive((a) => Math.min(xs.length - 1, a + 1)); }
    else if (event.key === 'ArrowLeft') { event.preventDefault(); setActive((a) => Math.max(0, (a < 0 ? xs.length : a) - 1)); }
    else if (event.key === 'Escape') setActive(-1);
  };

  const activeX = active >= 0 ? xs[active] : null;
  const readout = activeX == null ? [] : series.map((s) => ({ s, p: s.points.find((p) => p.x === activeX) })).filter((r) => r.p);
  const tipLeft = activeX == null ? 0 : (px(activeX) / W) * 100;
  const flip = tipLeft > 62;

  return (
    <figure className="chart" ref={wrap}>
      <figcaption>
        <span className="chart-title">{title}</span>
        {subtitle && <span className="chart-sub">{subtitle}</span>}
      </figcaption>
      <ul className="legend" aria-label="Legend">
        {series.map((s) => (
          <li key={s.id}>
            <svg width="26" height="12" viewBox="0 0 26 12" aria-hidden="true">
              <line x1="0" y1="6" x2="26" y2="6" stroke={s.color} strokeWidth="2" strokeLinecap="round" />
              <Marker shape={s.marker} x={13} y={6} color={s.color} />
            </svg>
            {s.name}
          </li>
        ))}
      </ul>
      <div className="chart-plot">
        <svg
          viewBox={`0 0 ${W} ${H}`}
          role="img"
          aria-labelledby={`${uid}-t`}
          tabIndex={0}
          onPointerMove={move}
          onPointerLeave={() => setActive(-1)}
          onFocus={() => setActive((a) => (a < 0 ? xs.length - 1 : a))}
          onBlur={() => setActive(-1)}
          onKeyDown={keys}
        >
          <title id={`${uid}-t`}>{`${title}. ${yLabel} against ${xLabel}. Use the left and right arrow keys to read values, or see the table below.`}</title>
          {yTicks.map((v) => (
            <g key={v}>
              <line className="grid" x1={M.left} x2={W - M.right} y1={py(v)} y2={py(v)} />
              <text className="tick" x={M.left - 9} y={py(v) + 4} textAnchor="end">{(formatYTick ?? String)(v)}</text>
            </g>
          ))}
          {xs.map((v) => (
            <text key={v} className="tick" x={px(v)} y={H - M.bottom + 20} textAnchor="middle">{formatX(v)}</text>
          ))}
          <line className="axis" x1={M.left} x2={W - M.right} y1={H - M.bottom} y2={H - M.bottom} />
          <text className="axis-title" x={M.left + PW / 2} y={H - 6} textAnchor="middle">{xLabel}</text>
          <text className="axis-title" transform={`translate(14 ${M.top + PH / 2}) rotate(-90)`} textAnchor="middle">{yLabel}</text>

          {activeX != null && <line className="crosshair" x1={px(activeX)} x2={px(activeX)} y1={M.top} y2={H - M.bottom} />}

          {series.map((s) => (
            <g key={s.id}>
              <polyline className="line" points={s.points.map((p) => `${px(p.x)},${py(p.y)}`).join(' ')} stroke={s.color} />
              {s.points.map((p) => (
                <g key={p.x} className="dot">
                  <Marker shape={s.marker} x={px(p.x)} y={py(p.y)} color={s.color} />
                </g>
              ))}
            </g>
          ))}
        </svg>
        {readout.length > 0 && (
          <div className={`tooltip${flip ? ' tooltip-left' : ''}`} style={{ left: `${tipLeft}%` }} role="status">
            <div className="tooltip-title">{formatX(activeX)} {xLabel.toLowerCase()}</div>
            {readout.map(({ s, p }) => (
              <div key={s.id} className="tooltip-row">
                <svg width="18" height="10" viewBox="0 0 18 10" aria-hidden="true"><line x1="0" y1="5" x2="18" y2="5" stroke={s.color} strokeWidth="2" strokeLinecap="round" /></svg>
                <strong>{formatY(p.y)}</strong>
                <span>{s.name}</span>
              </div>
            ))}
          </div>
        )}
      </div>
    </figure>
  );
}
