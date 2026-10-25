/**
 * The save module uses window.localStorage; node tests get a minimal Map-backed
 * implementation so persistence paths can be exercised without a browser.
 */
export function mockSafeStorage(): void {
  if (typeof globalThis.window === 'undefined') {
    const store = new Map<string, string>();
    const localStorage = {
      getItem: (k: string) => (store.has(k) ? store.get(k)! : null),
      setItem: (k: string, v: string) => void store.set(k, String(v)),
      removeItem: (k: string) => void store.delete(k),
      clear: () => store.clear(),
      key: (i: number) => Array.from(store.keys())[i] ?? null,
      get length() {
        return store.size;
      },
    };
    (globalThis as unknown as { window: Record<string, unknown> }).window = { localStorage };
  }
}
