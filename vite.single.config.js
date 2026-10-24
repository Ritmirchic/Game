import { defineConfig } from 'vite';
import { viteSingleFile } from 'vite-plugin-singlefile';

// Builds the entire game — code, styles and fonts — into one portable HTML file.
export default defineConfig({
  plugins: [viteSingleFile()],
  build: {
    outDir: 'dist-single',
    emptyOutDir: true,
  },
});
