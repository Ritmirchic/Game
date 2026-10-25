import { installAll } from './dom-setup';

// jsdom lacks a few APIs the renderer and audio engine probe for.
if (typeof HTMLCanvasElement !== 'undefined') installAll();

export {};
