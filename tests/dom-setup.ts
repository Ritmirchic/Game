/**
 * jsdom bootstrap for UI tests: gives the DOM a real 2D canvas (via
 * @napi-rs/canvas) plus the handful of browser APIs the game touches.
 */
import { createCanvas } from '@napi-rs/canvas';
import { vi } from 'vitest';

const contexts = new WeakMap<HTMLCanvasElement, unknown>();

function grunt(value: number) {
  return value > 0 ? value : 320;
}

export function installCanvas(): void {
  Object.defineProperty(HTMLCanvasElement.prototype, 'getContext', {
    configurable: true,
    value(this: HTMLCanvasElement, type: string) {
      if (type !== '2d') return null;
      let cached = contexts.get(this) as ReturnType<typeof createCanvas> | undefined;
      if (!cached) {
        cached = createCanvas(grunt(this.width), grunt(this.height)) as unknown as ReturnType<typeof createCanvas>;
        contexts.set(this, cached);
      }
      const ctx = (cached as unknown as { getContext: (t: string) => Record<string, unknown> }).getContext('2d');
      if (!ctx.__patched) {
        const original = (ctx.drawImage as (...args: unknown[]) => void).bind(ctx);
        ctx.drawImage = (img: unknown, ...rest: unknown[]) => {
          const resolved =
            img && typeof img === 'object' && (img as HTMLCanvasElement).tagName === 'CANVAS' ? contexts.get(img as HTMLCanvasElement) : img;
          if (!resolved) return;
          original(resolved, ...rest);
        };
        ctx.__patched = true;
      }
      return ctx;
    },
  });
  Object.defineProperty(HTMLCanvasElement.prototype, 'toDataURL', {
    configurable: true,
    value(this: HTMLCanvasElement) {
      const c = contexts.get(this) as unknown as { toDataURL?: (m: string) => string } | undefined;
      return c?.toDataURL?.('image/png') ?? 'data:image/png;base64,';
    },
  });
  Object.defineProperty(HTMLElement.prototype, 'getBoundingClientRect', {
    configurable: true,
    value(this: HTMLElement) {
      const w = this.tagName === 'CANVAS' ? 1000 : 320;
      const h = this.tagName === 'CANVAS' ? 760 : 200;
      return { x: 0, y: 0, left: 0, top: 0, right: w, bottom: h, width: w, height: h, toJSON() {} };
    },
  });
  if (!window.matchMedia) {
    Object.defineProperty(window, 'matchMedia', {
      configurable: true,
      value: () => ({ matches: false, addEventListener() {}, removeEventListener() {}, addListener() {}, removeListener() {} }),
    });
  }
}

class FakeNode {
  connect() {
    return this;
  }
  disconnect() {}
  start() {}
  stop() {}
  get gain() {
    return { value: 0, setValueAtTime() {}, exponentialRampToValueAtTime() {}, linearRampToValueAtTime() {} };
  }
  get frequency() {
    return { value: 0, setValueAtTime() {}, exponentialRampToValueAtTime() {} };
  }
  get detune() {
    return { value: 0 };
  }
  get Q() {
    return { value: 0 };
  }
}

export function installAudio(): void {
  class FakeAudioContext {
    currentTime = 0;
    sampleRate = 48000;
    destination = new FakeNode();
    state = 'running';
    createGain() {
      return new FakeNode();
    }
    createOscillator() {
      return new FakeNode();
    }
    createBufferSource() {
      return new FakeNode();
    }
    createBiquadFilter() {
      return new FakeNode();
    }
    createBuffer(_c: number, length: number) {
      return { getChannelData: () => new Float32Array(length) };
    }
    resume() {
      return Promise.resolve();
    }
  }
  (window as unknown as Record<string, unknown>).AudioContext = FakeAudioContext;
}

export function stubRaf(): void {
  // Keep the render loop out of the way: one frame at a time is plenty for tests.
  window.requestAnimationFrame = ((cb: FrameRequestCallback) => window.setTimeout(() => cb(performance.now()), 0)) as typeof window.requestAnimationFrame;
}

export function installAll(): void {
  installCanvas();
  installAudio();
  vi.stubGlobal('resizeTo', () => {});
}
