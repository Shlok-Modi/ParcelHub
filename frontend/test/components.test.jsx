import { describe, it, expect, vi, afterEach } from 'vitest';
import { render, screen, fireEvent } from '@testing-library/react';
import GraphBoard from '../src/components/GraphBoard.jsx';
import LineChart from '../src/charts/LineChart.jsx';
import App, { pageFromHash } from '../src/App.jsx';
import MergePanel from '../src/components/MergePanel.jsx';
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

describe('MergePanel', () => {
  const answer = (heap) => ({
    ok: true, command: 'merge', heap, hubA: { parcels: 3 }, hubB: { parcels: 3 }, merged: 6, verified: true,
    checks: { countsMatch: true, hubBEmpty: true, noParcelLost: true, noDuplicates: true, orderMatchesSortedKeys: true },
    dispatches: [
      { id: 102, level: 'Express', priority: 1, arrival: 1, deadline: '14:30', source: 1, destination: 7, reachable: true, path: [1, 3, 6, 5, 7], cost: 23, origin: 'B' },
      { id: 101, level: 'Standard', priority: 3, arrival: 0, deadline: '17:00', source: 1, destination: 5, reachable: true, path: [1, 3, 6, 5], cost: 20, origin: 'A' },
    ],
    heapOps: { inserts: 6, extracts: 6, decreaseKeys: 0, merges: 1, comparisons: 17 },
  });

  it('lists every parcel with a hub choice and the hub sizes', () => {
    render(<MergePanel city={demoCity()} />);
    expect(screen.getAllByRole('combobox', { name: /Hub for parcel/ })).toHaveLength(6);
    expect(screen.getByText(/Hub A: 3 parcels\. Hub B: 3 parcels\./)).toBeInTheDocument();
  });

  it('moving a parcel changes the hub sizes', () => {
    render(<MergePanel city={demoCity()} />);
    fireEvent.change(screen.getByRole('combobox', { name: 'Hub for parcel 101' }), { target: { value: 'B' } });
    expect(screen.getByText(/Hub A: 2 parcels\. Hub B: 4 parcels\./)).toBeInTheDocument();
  });

  it('sends both hubs to the server and shows the checks and the merged order', async () => {
    const fetchMock = vi.fn(async () => ({ ok: true, status: 200, json: async () => answer('Binary Heap') }));
    vi.stubGlobal('fetch', fetchMock);
    render(<MergePanel city={demoCity()} />);
    fireEvent.change(screen.getByRole('combobox', { name: 'Heap' }), { target: { value: 'binary' } });
    fireEvent.click(screen.getByRole('button', { name: 'Merge and dispatch' }));
    expect(await screen.findByText(/merged into one queue of 6/)).toBeInTheDocument();
    expect(screen.getByText(/Every check passed/)).toBeInTheDocument();
    expect(screen.getByText('from Hub B')).toBeInTheDocument();
    const [url, init] = fetchMock.mock.calls[0];
    expect(url).toBe('/api/merge');
    const body = JSON.parse(init.body);
    expect(body.hubA).toHaveLength(3);
    expect(body.hubB).toHaveLength(3);
  });

  it('shows the server error instead of a result', async () => {
    vi.stubGlobal('fetch', vi.fn(async () => ({ ok: false, status: 400, json: async () => ({ error: { code: 'invalid_request', message: 'hubB[0].id: duplicate parcel id 1' } }) })));
    render(<MergePanel city={demoCity()} />);
    fireEvent.click(screen.getByRole('button', { name: 'Merge and dispatch' }));
    expect(await screen.findByText(/duplicate parcel id 1/)).toBeInTheDocument();
    expect(screen.queryByText(/Every check passed/)).not.toBeInTheDocument();
  });

  it('cannot merge when there are no parcels', () => {
    render(<MergePanel city={{ ...demoCity(), parcels: [], upgrades: [] }} />);
    expect(screen.getByRole('button', { name: 'Merge and dispatch' })).toBeDisabled();
  });
});
