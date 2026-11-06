// The same city and parcels as the C++ demo, so the frontend can start with something to show.
export const SAMPLE = Object.freeze({
  graph: {
    nodes: 8, // H8 is deliberately isolated
    edges: [
      [1, 2, 7], [1, 3, 9], [1, 6, 14], [2, 3, 10], [2, 4, 15],
      [3, 4, 11], [3, 6, 2], [4, 5, 6], [5, 6, 9], [5, 7, 3],
    ].map(([from, to, weight]) => ({ from, to, weight })),
  },
  parcels: [
    { id: 101, source: 1, destination: 5, level: 'standard', deadline: '17:00', arrival: 0 },
    { id: 102, source: 1, destination: 7, level: 'express', deadline: '14:30', arrival: 1 },
    { id: 103, source: 2, destination: 4, level: 'same-day', deadline: '16:00', arrival: 2 },
    { id: 104, source: 3, destination: 5, level: 'express', deadline: '13:00', arrival: 3 },
    { id: 105, source: 1, destination: 8, level: 'standard', deadline: '18:00', arrival: 4 },
    { id: 106, source: 6, destination: 4, level: 'standard', deadline: '18:00', arrival: 5 },
  ],
  upgrades: [{ id: 106, level: 'express' }],
});
