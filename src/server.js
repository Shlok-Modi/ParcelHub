import fs from 'node:fs';
import { createApp } from './app.js';
import { loadConfig } from './config.js';

let config;
try {
  config = loadConfig();
} catch (err) {
  console.error(`Configuration error: ${err.message}`);
  process.exit(1);
}

if (!fs.existsSync(config.apiBinary)) {
  console.warn(`WARNING: calculation program not found at ${config.apiBinary}`);
  console.warn('Build it first (build.ps1) or set PARCELHUB_API_BIN. Calculation endpoints will return 503 until it exists.');
}

const server = createApp({ config }).listen(config.port, config.host, () => {
  console.log(`ParcelHub API listening on http://${config.host}:${config.port}`);
});

const shutdown = () => {
  console.log('Shutting down...');
  server.close(() => process.exit(0));
  setTimeout(() => process.exit(1), 5000).unref();
};
process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);
