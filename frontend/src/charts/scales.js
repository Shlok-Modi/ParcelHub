// Small scale and formatting helpers for the SVG charts.
export function niceStep(rough) {
  const pow = 10 ** Math.floor(Math.log10(rough));
  const f = rough / pow;
  const nice = f <= 1 ? 1 : f <= 2 ? 2 : f <= 2.5 ? 2.5 : f <= 5 ? 5 : 10;
  return nice * pow;
}

// Ticks from 0 up to a round maximum that is >= max.
export function linearTicks(max, count = 4) {
  if (!(max > 0)) return { ticks: [0, 1], max: 1 };
  const step = niceStep(max / count);
  const top = Math.ceil(max / step - 1e-9) * step;
  const ticks = [];
  for (let v = 0; v <= top + step / 1e6; v += step) ticks.push(Number(v.toPrecision(12)));
  return { ticks, max: top };
}

// Powers of ten that cover [min, max] (both > 0).
export function logTicks(min, max) {
  const lo = Math.floor(Math.log10(Math.max(min, 1e-9)));
  let hi = Math.ceil(Math.log10(Math.max(max, min * 1.0001)));
  if (hi === lo) hi = lo + 1;
  const ticks = [];
  for (let e = lo; e <= hi; e++) ticks.push(Number((10 ** e).toPrecision(12)));
  return { ticks, min: ticks[0], max: ticks[ticks.length - 1] };
}

export const clamp01 = (t) => Math.min(1, Math.max(0, t));
export const linearPos = (v, min, max) => (max === min ? 0.5 : (v - min) / (max - min));
export const logPos = (v, min, max) => (max === min ? 0.5 : (Math.log10(Math.max(v, min)) - Math.log10(min)) / (Math.log10(max) - Math.log10(min)));

export function compact(n) {
  const abs = Math.abs(n);
  if (abs >= 1e6) return `${trim(n / 1e6)}M`;
  if (abs >= 1e3) return `${trim(n / 1e3)}K`;
  return String(trim(n));
}
const trim = (n) => Number(n.toFixed(2));

// Milliseconds with a sensible number of digits: 0.021 ms, 2.5 ms, 120 ms, 1.2 s.
export function formatMs(ms) {
  if (!Number.isFinite(ms)) return '–';
  if (ms >= 1000) return `${trim(ms / 1000)} s`;
  if (ms >= 100) return `${Math.round(ms)} ms`;
  if (ms >= 10) return `${ms.toFixed(1)} ms`;
  if (ms >= 1) return `${ms.toFixed(2)} ms`;
  if (ms >= 0.1) return `${ms.toFixed(3)} ms`;
  return `${ms.toFixed(4)} ms`;
}

// Axis tick label for a time value (unit is in the axis title, so no suffix).
export function formatTick(v) {
  if (v === 0) return '0';
  if (v >= 1000) return compact(v);
  if (v >= 1) return String(Number(v.toFixed(2)));
  return String(Number(v.toPrecision(2)));
}

export const formatCount = (n) => Math.round(n).toLocaleString('en');
