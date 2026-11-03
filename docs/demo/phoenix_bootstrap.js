// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Phoenix web bootstrap (the analogue of flutter_bootstrap.js).
// `phoenix build web` replaces the config line below and copies this file next
// to index.html. It:
//   1. ensures cross-origin isolation (registers phoenix_sw.js if needed),
//   2. checks browser capabilities (wasm64, SharedArrayBuffer, WebGPU),
//   3. loads the engine module with the right canvas and logging hooks,
//   4. in `phoenix run`, listens for hot-restart events from the dev server.
(function () {
  'use strict';
  const config = {"appJs":"hello_phoenix.js","buildMode":"release","devServer":false,"engineVersion":"0.1.0"};

  const g = globalThis;
  const phoenix = (g.phoenix = g.phoenix || {});
  phoenix.config = config;
  phoenix.status = 'booting';

  const RELOAD_KEY = 'phoenix-coi-reload';
  const MEMORY64_PROBE = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 5, 3, 1, 4, 1]);

  function overlay(message, isError) {
    let el = document.getElementById('phoenix-overlay');
    if (!el) {
      el = document.createElement('div');
      el.id = 'phoenix-overlay';
      el.setAttribute('role', 'status');
      el.style.cssText =
        'position:fixed;inset:auto 0 0 0;padding:10px 16px;font:13px/1.4 system-ui,sans-serif;' +
        'color:#fff;background:rgba(16,16,24,.85);z-index:10;white-space:pre-wrap';
      document.body.appendChild(el);
    }
    el.style.background = isError ? 'rgba(160,30,30,.95)' : 'rgba(16,16,24,.85)';
    el.textContent = message;
    el.hidden = !message;
  }

  function fail(message) {
    phoenix.status = 'error';
    phoenix.error = message;
    console.error('[phoenix]', message);
    overlay(message, true);
  }

  async function ensureCrossOriginIsolated() {
    if (g.crossOriginIsolated) {
      sessionStorage.removeItem(RELOAD_KEY);
      return true;
    }
    if (!('serviceWorker' in navigator)) return false;
    if (sessionStorage.getItem(RELOAD_KEY)) {
      sessionStorage.removeItem(RELOAD_KEY);
      return false; // we already tried once; avoid reload loops
    }
    overlay('Enabling cross-origin isolation…');
    const registration = await navigator.serviceWorker.register('phoenix_sw.js');
    sessionStorage.setItem(RELOAD_KEY, '1');
    if (registration.active && !navigator.serviceWorker.controller) {
      location.reload();
    } else {
      navigator.serviceWorker.addEventListener('controllerchange', () => location.reload());
    }
    return null; // a reload is pending
  }

  function capabilities() {
    return {
      memory64: typeof WebAssembly === 'object' && WebAssembly.validate(MEMORY64_PROBE),
      sharedArrayBuffer: typeof SharedArrayBuffer === 'function',
      webgpu: 'gpu' in navigator,
      opfs: !!(navigator.storage && navigator.storage.getDirectory),
      cores: navigator.hardwareConcurrency || 0,
    };
  }

  function connectDevServer() {
    if (!config.devServer || typeof EventSource !== 'function') return;
    const events = new EventSource('/__phoenix/events');
    events.addEventListener('reload', () => location.reload());
    events.addEventListener('error', () => overlay('Lost connection to `phoenix run`.', true));
  }

  async function boot() {
    const coi = await ensureCrossOriginIsolated();
    if (coi === null) return;
    const caps = (phoenix.capabilities = capabilities());
    if (!coi || !caps.sharedArrayBuffer) {
      return fail('Phoenix needs SharedArrayBuffer (cross-origin isolation) for its worker threads.\n' +
                  'Serve with COOP/COEP headers (phoenix run does) or allow the service worker.');
    }
    if (!caps.memory64) {
      return fail('Phoenix needs WebAssembly Memory64 (wasm64). Use a current Chrome, Edge or Firefox.');
    }
    if (!caps.webgpu) console.warn('[phoenix] WebGPU unavailable; falling back to WebGL.');

    const canvas = document.getElementById('phoenix-canvas');
    overlay('Loading engine…');
    g.Module = {
      canvas,
      print: (text) => console.log(text),
      printErr: (text) => console.warn(text),
      setStatus: (text) => text && overlay(text),
      onAbort: (what) => fail('Engine aborted: ' + what),
      onRuntimeInitialized: () => {
        phoenix.status = 'running';
        overlay('');
      },
    };
    const script = document.createElement('script');
    script.src = config.appJs;
    script.onerror = () => fail('Could not load ' + config.appJs);
    document.body.appendChild(script);
    connectDevServer();
  }

  boot().catch((e) => fail(String(e && e.stack ? e.stack : e)));
})();
