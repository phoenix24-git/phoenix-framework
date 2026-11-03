// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// End-to-end tests for a Phoenix web build. Two servers are started:
//   - `phoenix run -d web-server` (the real dev server: COOP/COEP headers, SSE, DevTools)
//   - a plain static server without isolation headers, emulating GitHub Pages so the
//     service-worker fallback in phoenix_bootstrap.js is exercised.
// PHOENIX_E2E_APP selects the app (default: examples/hello_phoenix, built beforehand).
import { defineConfig, devices } from '@playwright/test';
import * as path from 'node:path';

const repo = path.resolve(__dirname, '../..');
const app = process.env.PHOENIX_E2E_APP ?? path.join(repo, 'examples/hello_phoenix');
const mode = process.env.PHOENIX_E2E_MODE ?? '--release';

const webgpuArgs = [
  '--enable-unsafe-webgpu',
  '--enable-features=Vulkan',
  '--use-vulkan=swiftshader',
  '--use-webgpu-adapter=swiftshader',
  '--use-angle=swiftshader',
];

export default defineConfig({
  testDir: './tests',
  timeout: 90_000,
  expect: { timeout: 30_000 },
  fullyParallel: false,
  workers: 1,
  retries: process.env.CI ? 1 : 0,
  reporter: process.env.CI ? [['github'], ['html', { open: 'never' }]] : 'list',
  use: {
    baseURL: 'http://127.0.0.1:8123',
    viewport: { width: 1024, height: 700 },
    trace: 'retain-on-failure',
  },
  projects: [
    { name: 'chromium-webgpu', use: { ...devices['Desktop Chrome'], viewport: { width: 1024, height: 700 }, launchOptions: { args: webgpuArgs } } },
    { name: 'chromium-webgl-fallback', use: { ...devices['Desktop Chrome'], viewport: { width: 1024, height: 700 } } },
  ],
  webServer: [
    {
      command: `"${path.join(repo, 'bin/phoenix')}" run -d web-server --web-port 8123 ${mode}`,
      cwd: app,
      url: 'http://127.0.0.1:8123/',
      timeout: 900_000,
      reuseExistingServer: !process.env.CI,
      stdout: 'pipe',
    },
    {
      command: `python3 -m http.server 8124 --bind 127.0.0.1 --directory "${path.join(app, 'build/web')}"`,
      url: 'http://127.0.0.1:8124/',
      timeout: 60_000,
      reuseExistingServer: !process.env.CI,
    },
  ],
});
