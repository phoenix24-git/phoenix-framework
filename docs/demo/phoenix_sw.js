// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Phoenix cross-origin-isolation service worker.
//
// wasm64 + pthreads need SharedArrayBuffer, which browsers only enable on
// cross-origin isolated pages (COOP: same-origin + COEP: require-corp).
// Static hosts such as GitHub Pages cannot set those headers, so this worker
// re-serves every same-scope response with them added. `phoenix run` sets the
// headers itself, so the worker is a no-op there.
self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', (event) => event.waitUntil(self.clients.claim()));

self.addEventListener('fetch', (event) => {
  const request = event.request;
  if (request.cache === 'only-if-cached' && request.mode !== 'same-origin') return;
  event.respondWith(
    fetch(request)
      .then((response) => {
        if (response.status === 0) return response;
        const headers = new Headers(response.headers);
        headers.set('Cross-Origin-Embedder-Policy', 'require-corp');
        headers.set('Cross-Origin-Opener-Policy', 'same-origin');
        headers.set('Cross-Origin-Resource-Policy', 'cross-origin');
        return new Response(response.body, {
          status: response.status,
          statusText: response.statusText,
          headers,
        });
      })
      .catch((error) => {
        console.error('[phoenix_sw]', error);
        return Response.error();
      }),
  );
});
