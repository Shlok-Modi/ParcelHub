// Deterministic map layout (Fruchterman-Reingold). Hubs that already have a position stay
// where they are; only new hubs are placed and relaxed, so editing the city never shuffles the map.
const PAD = 0.04;
const clamp = (v) => Math.min(1 - PAD, Math.max(PAD, v));

export function layoutGraph(nodeCount, edges, existing = {}) {
  const pos = [];
  const pinned = [];
  const free = [];
  for (let i = 1; i <= nodeCount; i++) {
    const p = existing[i];
    if (p && Number.isFinite(p.x) && Number.isFinite(p.y)) { pos[i] = { x: clamp(p.x), y: clamp(p.y) }; pinned[i] = true; }
    else free.push(i);
  }
  if (free.length === 0) return Object.fromEntries(pos.map((p, i) => [i, p]).filter(([i]) => i > 0));

  // Start free hubs on a circle (deterministic), each at an angle from its place in the list.
  free.forEach((h, k) => {
    const a = (2 * Math.PI * k) / free.length + 0.5;
    pos[h] = { x: 0.5 + 0.38 * Math.cos(a), y: 0.5 + 0.38 * Math.sin(a) };
  });

  const k = Math.sqrt(1 / Math.max(nodeCount, 1)) * 0.9;
  const iterations = 200;
  let temp = 0.1;
  for (let it = 0; it < iterations; it++) {
    const disp = Array.from({ length: nodeCount + 1 }, () => ({ x: 0, y: 0 }));
    for (let i = 1; i <= nodeCount; i++) {
      for (let j = i + 1; j <= nodeCount; j++) {
        let dx = pos[i].x - pos[j].x, dy = pos[i].y - pos[j].y;
        let d = Math.hypot(dx, dy);
        if (d < 1e-4) { dx = 1e-3 * (i - j); dy = 1e-3; d = Math.hypot(dx, dy); }
        const f = (k * k) / d;
        disp[i].x += (dx / d) * f; disp[i].y += (dy / d) * f;
        disp[j].x -= (dx / d) * f; disp[j].y -= (dy / d) * f;
      }
    }
    for (const e of edges) {
      if (e.from === e.to || e.from > nodeCount || e.to > nodeCount) continue;
      const dx = pos[e.from].x - pos[e.to].x, dy = pos[e.from].y - pos[e.to].y;
      const d = Math.max(Math.hypot(dx, dy), 1e-4);
      const f = (d * d) / k;
      disp[e.from].x -= (dx / d) * f; disp[e.from].y -= (dy / d) * f;
      disp[e.to].x += (dx / d) * f; disp[e.to].y += (dy / d) * f;
    }
    for (const h of free) {
      const d = Math.max(Math.hypot(disp[h].x, disp[h].y), 1e-9);
      const step = Math.min(d, temp);
      pos[h] = { x: clamp(pos[h].x + (disp[h].x / d) * step), y: clamp(pos[h].y + (disp[h].y / d) * step) };
    }
    temp *= 0.97;
  }
  return Object.fromEntries(pos.map((p, i) => [i, p]).filter(([i]) => i > 0));
}

// Map-space helpers: positions are fractions; the board draws them inside a padded viewBox.
export const BOARD = Object.freeze({ width: 1000, height: 620, padX: 70, padY: 56 });
export const toBoard = (p) => ({ x: BOARD.padX + p.x * (BOARD.width - 2 * BOARD.padX), y: BOARD.padY + p.y * (BOARD.height - 2 * BOARD.padY) });
export const fromBoard = (x, y) => ({
  x: clamp((x - BOARD.padX) / (BOARD.width - 2 * BOARD.padX)),
  y: clamp((y - BOARD.padY) / (BOARD.height - 2 * BOARD.padY)),
});
