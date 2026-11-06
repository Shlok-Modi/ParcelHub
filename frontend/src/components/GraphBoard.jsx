import { useRef } from 'react';
import { hubLabel } from '../city.js';
import { BOARD, fromBoard, toBoard } from '../layout.js';

const edgeKey = (a, b) => (a < b ? `${a}-${b}` : `${b}-${a}`);

// The city map, drawn like a depot route map: roads are thick lines with cost plates,
// hubs are stations, the chosen route is a heavy line with a white casing.
export default function GraphBoard({
  nodes, edges, positions,
  path = [], tree = [], source = 0, destination = 0,
  unreachable = null, badges = null,
  onMove, onPick, label = 'City map',
}) {
  const svgRef = useRef(null);
  const drag = useRef(null);

  const pathKeys = new Set();
  for (let i = 1; i < path.length; i++) pathKeys.add(edgeKey(path[i - 1], path[i]));
  const treeKeys = new Set(tree.map((e) => edgeKey(e.from, e.to)));
  const onPath = new Set(path);
  const showWeights = edges.length <= 60;

  const point = (h) => toBoard(positions[h] ?? { x: 0.5, y: 0.5 });

  function boardCoords(event) {
    const svg = svgRef.current;
    if (!svg?.createSVGPoint || !svg.getScreenCTM()) return null;
    const pt = svg.createSVGPoint();
    pt.x = event.clientX; pt.y = event.clientY;
    return pt.matrixTransform(svg.getScreenCTM().inverse());
  }
  function down(event, hub) {
    event.currentTarget.setPointerCapture?.(event.pointerId);
    drag.current = { hub, x: event.clientX, y: event.clientY, moved: false };
  }
  function move(event) {
    const d = drag.current;
    if (!d) return;
    if (!d.moved && Math.hypot(event.clientX - d.x, event.clientY - d.y) < 5) return;
    d.moved = true;
    const p = boardCoords(event);
    if (p && onMove) onMove(d.hub, fromBoard(p.x, p.y));
  }
  function up() {
    const d = drag.current;
    drag.current = null;
    if (d && !d.moved) onPick?.(d.hub);
  }
  function key(event, hub) {
    if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); onPick?.(hub); return; }
    const step = 0.025;
    const delta = { ArrowLeft: [-step, 0], ArrowRight: [step, 0], ArrowUp: [0, -step], ArrowDown: [0, step] }[event.key];
    if (delta && onMove) {
      event.preventDefault();
      const p = positions[hub] ?? { x: 0.5, y: 0.5 };
      onMove(hub, { x: Math.min(0.96, Math.max(0.04, p.x + delta[0])), y: Math.min(0.96, Math.max(0.04, p.y + delta[1])) });
    }
  }

  // Parallel roads between the same two hubs are drawn side by side so none is hidden.
  const seen = new Map();
  const roads = edges.map((e, i) => {
    const k = edgeKey(e.from, e.to);
    const index = seen.get(k) ?? 0;
    seen.set(k, index + 1);
    return { ...e, i, k, index };
  });

  const hubs = Array.from({ length: nodes }, (_, i) => i + 1);

  return (
    <svg
      ref={svgRef}
      className="board"
      viewBox={`0 0 ${BOARD.width} ${BOARD.height}`}
      role="group"
      aria-label={label}
    >
      <g className="roads">
        {roads.map((r) => {
          const a = point(r.from), b = point(r.to);
          const nx = -(b.y - a.y), ny = b.x - a.x, len = Math.hypot(nx, ny) || 1;
          const off = r.index * 10;
          const dx = (nx / len) * off, dy = (ny / len) * off;
          return <line key={r.i} className="road" x1={a.x + dx} y1={a.y + dy} x2={b.x + dx} y2={b.y + dy} />;
        })}
      </g>
      <g className="tree-layer">
        {roads.filter((r) => treeKeys.has(r.k) && r.index === 0).map((r) => {
          const a = point(r.from), b = point(r.to);
          return <line key={r.i} className="road-tree" x1={a.x} y1={a.y} x2={b.x} y2={b.y} />;
        })}
      </g>
      <g className="path-layer">
        {roads.filter((r) => pathKeys.has(r.k) && r.index === 0).map((r) => {
          const a = point(r.from), b = point(r.to);
          return (
            <g key={r.i}>
              <line className="road-casing" x1={a.x} y1={a.y} x2={b.x} y2={b.y} />
              <line className="road-path" x1={a.x} y1={a.y} x2={b.x} y2={b.y} />
            </g>
          );
        })}
      </g>
      <g className="core-layer">
        {roads.filter((r) => treeKeys.has(r.k) && pathKeys.has(r.k) && r.index === 0).map((r) => {
          const a = point(r.from), b = point(r.to);
          return <line key={r.i} className="road-tree-core" x1={a.x} y1={a.y} x2={b.x} y2={b.y} />;
        })}
      </g>
      {showWeights && (
        <g className="plates" aria-hidden="true">
          {roads.map((r) => {
            const a = point(r.from), b = point(r.to);
            const nx = -(b.y - a.y), ny = b.x - a.x, len = Math.hypot(nx, ny) || 1;
            const off = r.index * 10;
            const cx = (a.x + b.x) / 2 + (nx / len) * off, cy = (a.y + b.y) / 2 + (ny / len) * off;
            const text = String(r.weight);
            const w = Math.max(24, text.length * 9 + 12);
            const hot = pathKeys.has(r.k) && r.index === 0;
            return (
              <g key={r.i} className={hot ? 'plate plate-hot' : 'plate'}>
                <rect x={cx - w / 2} y={cy - 11} width={w} height={22} rx={5} />
                <text x={cx} y={cy + 5} textAnchor="middle">{text}</text>
              </g>
            );
          })}
        </g>
      )}
      <g className="hubs">
        {hubs.map((h) => {
          const p = point(h);
          const isSource = h === source, isDest = h === destination && destination !== 0;
          const dead = unreachable?.has(h);
          const cls = ['hub', isSource && 'hub-source', isDest && 'hub-dest', onPath.has(h) && 'hub-onpath', dead && 'hub-dead', onPick && 'hub-pick']
            .filter(Boolean).join(' ');
          const state = isSource ? ', route start' : isDest ? ', route end' : dead ? ', not reachable' : '';
          return (
            <g
              key={h}
              className={cls}
              transform={`translate(${p.x} ${p.y})`}
              tabIndex={0}
              role="button"
              aria-label={`Hub ${hubLabel(h)}${state}${badges?.[h] != null ? `, distance ${badges[h]}` : ''}`}
              onPointerDown={(e) => down(e, h)}
              onPointerMove={move}
              onPointerUp={up}
              onPointerCancel={() => { drag.current = null; }}
              onKeyDown={(e) => key(e, h)}
            >
              {isDest && <circle className="hub-ring" r={28} />}
              <circle className="hub-dot" r={21} />
              <text className="hub-name" y={6} textAnchor="middle">{hubLabel(h)}</text>
              {badges?.[h] != null && <text className="hub-badge" y={42} textAnchor="middle">{badges[h]}</text>}
            </g>
          );
        })}
      </g>
    </svg>
  );
}
