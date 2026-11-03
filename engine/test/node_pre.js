// Minimal browser shims so SDL's Emscripten backend can initialise under Node.
if (typeof window === 'undefined') {
  globalThis.window = { location: { search: '' }, prompt: () => null };
}
