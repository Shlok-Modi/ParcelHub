// Safe execution of the C++ program.
//  - spawn() with shell:false and a fixed argument list: no client data reaches a command line.
//  - client data goes only through stdin, as validated numbers and fixed words.
//  - hard timeout (process is killed), output size cap (killed when exceeded),
//    a concurrency limit with a bounded queue, and a minimal environment.
import { spawn } from 'node:child_process';
import { BusyError } from './errors.js';

export class RunnerError extends Error {
  constructor(kind, message) {
    super(message);
    this.name = 'RunnerError';
    this.kind = kind; // 'timeout' | 'output_limit' | 'unavailable'
  }
}

// Only what the program needs to start. On Windows the MinGW runtime DLLs are found via PATH.
function minimalEnv() {
  const env = {};
  for (const key of ['PATH', 'SystemRoot', 'TEMP', 'TMP']) {
    if (process.env[key] !== undefined) env[key] = process.env[key];
  }
  return env;
}

export function runProcess({ command, args = [], input = '', timeoutMs, maxOutputBytes, env = minimalEnv() }) {
  return new Promise((resolve, reject) => {
    let child;
    try {
      child = spawn(command, args, { shell: false, windowsHide: true, env, stdio: ['pipe', 'pipe', 'pipe'] });
    } catch (err) {
      reject(new RunnerError('unavailable', `cannot start the calculation program: ${err.code || err.message}`));
      return;
    }

    let stdout = '';
    let stderr = '';
    let bytes = 0;
    let settled = false;
    let failure = null;

    const stop = (kind, message) => {
      if (failure) return;
      failure = new RunnerError(kind, message);
      child.kill('SIGKILL');
    };
    const timer = setTimeout(() => stop('timeout', `calculation exceeded ${timeoutMs} ms`), timeoutMs);

    const collect = (which) => (chunk) => {
      bytes += Buffer.byteLength(chunk);
      if (bytes > maxOutputBytes) {
        stop('output_limit', `calculation produced more than ${maxOutputBytes} bytes of output`);
        return;
      }
      if (which === 'out') stdout += chunk; else stderr += chunk;
    };
    child.stdout.setEncoding('utf8');
    child.stderr.setEncoding('utf8');
    child.stdout.on('data', collect('out'));
    child.stderr.on('data', collect('err'));

    const done = (fn) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      fn();
    };
    child.on('error', (err) => {
      failure = failure || new RunnerError('unavailable', `cannot start the calculation program: ${err.code || err.message}`);
      done(() => reject(failure));
    });
    child.on('close', (code, signal) => {
      done(() => (failure ? reject(failure) : resolve({ code, signal, stdout, stderr })));
    });

    // The program may exit before reading all input (e.g. it rejected the request): ignore EPIPE.
    child.stdin.on('error', () => {});
    child.stdin.end(input);
  });
}

// Runs jobs through runProcess with at most `maxConcurrent` at once and `maxQueue` waiting.
export function createRunner({ command, args = [], timeoutMs, maxOutputBytes, maxConcurrent, maxQueue }) {
  let active = 0;
  const waiting = [];

  const release = () => {
    active -= 1;
    const next = waiting.shift();
    if (next) { active += 1; next(); }
  };
  const acquire = () => {
    if (active < maxConcurrent) { active += 1; return Promise.resolve(); }
    if (waiting.length >= maxQueue) return Promise.reject(new BusyError());
    return new Promise((resolve) => waiting.push(resolve));
  };

  return {
    async run(input) {
      await acquire();
      try {
        return await runProcess({ command, args, input, timeoutMs, maxOutputBytes });
      } finally {
        release();
      }
    },
    stats: () => ({ active, waiting: waiting.length }),
  };
}
