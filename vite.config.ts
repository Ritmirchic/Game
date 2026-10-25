import { defineConfig } from 'vite';

export default defineConfig({
  // Relative base keeps the build working both on GitHub Pages project
  // sub-paths (/repo/) and when the bundle is opened from a plain static host.
  base: './',
  server: {
    host: '0.0.0.0',
    // Previews are served from *.e2b.app proxies and arbitrary static hosts.
    allowedHosts: true,
    headers: { 'X-Frame-Options': 'ALLOWALL' },
  },
  preview: {
    host: '0.0.0.0',
    allowedHosts: true,
    headers: { 'X-Frame-Options': 'ALLOWALL' },
  },
  build: {
    target: 'es2020',
    outDir: 'dist',
    assetsDir: 'assets',
    cssCodeSplit: false,
    reportCompressedSize: false,
  },
  test: {
    environment: 'node',
    include: ['tests/**/*.test.ts'],
    setupFiles: ['tests/vitest.setup.ts'],
  },
});
