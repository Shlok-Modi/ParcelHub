import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import GraphBoard from '../src/components/GraphBoard.jsx';
import LineChart from '../src/charts/LineChart.jsx';
import App, { pageFromHash } from '../src/App.jsx';
import { demoCity } from '../src/city.js';

afterEach(() => { vi.unstubAllGlobals(); window.location.hash = ''; });

describe('GraphBoard', () => {
  const c = demoCity();
  it('draws every hub and road, and reports picks', () => {
    const onPick = vi.fn();
    const { container } = render(<GraphBoard nodes={c.nodes} edges={c.edges} positions={c.positions} onPick={onPick} />);
    expect(container.querySelectorAll('.hub').length).toBe(8);
    const hub = container.querySelectorAll('.hub')[2];
    fireEvent.keyDown(hub, { key: 'Enter' });
    expect(onPick).toHaveBeenCalledWith(3);
  });
  it('moves a hub with the arrow keys', () => {
    const onMove = vi.fn();
    const { container } = render(<GraphBoard nodes={c.nodes} edges={c.edges} positions={c.positions} onMove={onMove} />);
    fireEvent.keyDown(container.querySelectorAll('.hub')[0], { key: 'ArrowRight' });
    expect(onMove).toHaveBeenCalledWith(1, { x: 0.125, y: 0.28 });
  });
  it('marks the route', () => {
    const { container } = render(<GraphBoard nodes={c.nodes} edges={c.edges} positions={c.positions} path={[1, 3, 6]} source={1} destination={6} />);
    expect(container.querySelectorAll('.road-path').length).toBeGreaterThan(0);
  });
});

describe('LineChart', () => {
  const series = [
    { id: 'binary', name: 'Binary Heap', color: '#1d6fd8', marker: 'circle', points: [{ x: 100, y: 1 }, { x: 1000, y: 4 }] },
    { id: 'binomial', name: 'Binomial Heap', color: '#d9531e', marker: 'square', points: [{ x: 100, y: 2 }, { x: 1000, y: 3 }] },
  ];
  it('shows a legend with every series and an accessible title', () => {
    render(<LineChart title="Time" series={series} xValues={[100, 1000]} xLabel="Hubs" yLabel="Milliseconds" formatY={(v) => `${v} ms`} />);
    expect(screen.getByText('Binary Heap')).toBeInTheDocument();
    expect(screen.getByText('Binomial Heap')).toBeInTheDocument();
    expect(screen.getByText('Time')).toBeInTheDocument();
  });
  it('renders with a single size and in log mode without crashing', () => {
    const one = series.map((s) => ({ ...s, points: [s.points[0]] }));
    render(<LineChart title="One" series={one} xValues={[100]} xLabel="Hubs" yLabel="ms" formatY={String} logY />);
    expect(screen.getByText('One')).toBeInTheDocument();
  });
});

describe('App', () => {
  it('maps hashes to pages', () => {
    expect(pageFromHash('#/dispatch')).toBe('dispatch');
    expect(pageFromHash('#/nope')).toBe('routes');
    expect(pageFromHash('')).toBe('routes');
  });
  it('says so when the server cannot be reached', async () => {
    vi.stubGlobal('fetch', vi.fn(async () => { throw new TypeError('failed'); }));
    render(<App />);
    expect(await screen.findAllByText(/server/i)).not.toHaveLength(0);
    expect(screen.getByRole('link', { name: 'Dispatch' })).toBeInTheDocument();
  });
});
