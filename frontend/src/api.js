// Thin client for the ParcelHub backend. Every failure becomes an ApiError with a message
// that can be shown to the user as-is.
export class ApiError extends Error {
  constructor(message, { status = 0, code = 'error' } = {}) {
    super(message);
    this.name = 'ApiError';
    this.status = status;
    this.code = code;
  }
}

const NETWORK_MESSAGE = "Can't reach the calculation server. Open the backend folder and run npm start.";

export async function request(path, { method = 'GET', body, signal, timeoutMs = 30_000, as = 'json' } = {}) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(new Error('timeout')), timeoutMs);
  const onAbort = () => controller.abort(signal.reason);
  if (signal) {
    if (signal.aborted) controller.abort(signal.reason); else signal.addEventListener('abort', onAbort, { once: true });
  }
  let res;
  try {
    res = await fetch(path, {
      method,
      headers: body === undefined ? undefined : { 'content-type': 'application/json' },
      body: body === undefined ? undefined : JSON.stringify(body),
      signal: controller.signal,
    });
  } catch (err) {
    if (signal?.aborted) throw err;                  // the caller cancelled: let it handle that
    if (controller.signal.aborted) throw new ApiError('The server took too long to answer.', { code: 'timeout' });
    throw new ApiError(NETWORK_MESSAGE, { code: 'network' });
  } finally {
    clearTimeout(timer);
    signal?.removeEventListener('abort', onAbort);
  }

  if (!res.ok) {
    let message = `The server answered with an error (${res.status}).`;
    let code = 'error';
    let structured = false;
    try {
      const data = await res.json();
      if (data?.error?.message) { message = data.error.message; code = data.error.code || code; structured = true; }
    } catch { /* not JSON: keep the generic message */ }
    // The backend always answers with JSON errors. A bare 5xx means a proxy or gateway could not reach it.
    if (!structured && res.status >= 500) { message = NETWORK_MESSAGE; code = 'network'; }
    throw new ApiError(message, { status: res.status, code });
  }
  try {
    return as === 'text' ? await res.text() : await res.json();
  } catch {
    throw new ApiError('The server sent an answer that could not be read.', { status: res.status, code: 'bad_response' });
  }
}

export const getHealth = (opts) => request('/api/health', opts);
export const getSample = (opts) => request('/api/sample', opts);
export const postRoute = (body, opts) => request('/api/route', { ...opts, method: 'POST', body });
export const postMst = (body, opts) => request('/api/mst', { ...opts, method: 'POST', body });
export const postDispatch = (body, opts) => request('/api/dispatch', { ...opts, method: 'POST', body });
export const getBenchmarkResults = (opts) => request('/api/benchmark/results', opts);
export const runBenchmark = (body, opts) => request('/api/benchmark/run', { timeoutMs: 150_000, ...opts, method: 'POST', body });
