import { useEffect, useRef, useState } from 'react';

// Runs `fn(signal)` whenever `deps` change (after `delay` ms), cancelling the previous call.
// While a new request is in flight the previous data stays, so the screen does not flash.
export function useRequest(fn, deps, { delay = 0, enabled = true } = {}) {
  const [state, setState] = useState({ data: null, error: null, loading: false });
  const latest = useRef(fn);
  latest.current = fn;
  useEffect(() => {
    if (!enabled) { setState({ data: null, error: null, loading: false }); return undefined; }
    const controller = new AbortController();
    setState((s) => ({ ...s, loading: true, error: null }));
    const timer = setTimeout(async () => {
      try {
        const data = await latest.current(controller.signal);
        if (!controller.signal.aborted) setState({ data, error: null, loading: false });
      } catch (error) {
        if (!controller.signal.aborted) setState({ data: null, error, loading: false });
      }
    }, delay);
    return () => { clearTimeout(timer); controller.abort(); };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [...deps, enabled, delay]);
  return state;
}
