import react from '@vitejs/plugin-react';
import { defineConfig } from 'vite';

// In development the page is served by Vite and /api is forwarded to the Express backend,
// so the browser sees one origin (no CORS setup needed).
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: { '/api': { target: process.env.PARCELHUB_API_URL || 'http://127.0.0.1:3001', changeOrigin: false } },
  },
  test: {
    environment: 'jsdom',
    globals: true,
    setupFiles: ['./test/setup.js'],
    include: ['test/**/*.test.{js,jsx}'],
  },
});
