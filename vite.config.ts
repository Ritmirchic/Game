import { createReadStream, statSync } from 'node:fs';
import type { IncomingMessage, ServerResponse } from 'node:http';
import { fileURLToPath } from 'node:url';
import { defineConfig, type Plugin } from 'vite';

const standaloneFile = fileURLToPath(new URL('./last-beacon.html', import.meta.url));
const downloadHandler = (_request: IncomingMessage, response: ServerResponse): void => {
  response.statusCode = 200;
  response.setHeader('Content-Type', 'text/html; charset=utf-8');
  response.setHeader('Content-Disposition', 'attachment; filename="last-beacon.html"');
  response.setHeader('Content-Length', statSync(standaloneFile).size);
  response.setHeader('Cache-Control', 'no-store');
  createReadStream(standaloneFile).pipe(response);
};

const standaloneDownload: Plugin = {
  name: 'standalone-html-download',
  configureServer(server) {
    server.middlewares.use('/download/last-beacon.html', downloadHandler);
  },
  configurePreviewServer(server) {
    server.middlewares.use('/download/last-beacon.html', downloadHandler);
  },
};

export default defineConfig({
  plugins: [standaloneDownload],
  server: {
    host: '0.0.0.0',
    allowedHosts: true,
  },
  preview: {
    host: '0.0.0.0',
    allowedHosts: true,
  },
  build: {
    chunkSizeWarningLimit: 9000,
  },
});
