// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
import { expect, test } from '@playwright/test';
import { bootApp, closeTo, readOpfs, samplePixels, stats, tapFab } from './helpers';

test.describe('Phoenix engine on the web', () => {
  test('boots wasm64 with the 6-core virtual console profile', async ({ page }) => {
    const s = await bootApp(page);
    const info = await page.evaluate(() => (globalThis as any).phoenix.info);
    const caps = await page.evaluate(() => (globalThis as any).phoenix.capabilities);
    expect(await page.evaluate(() => crossOriginIsolated)).toBe(true);
    expect(caps.memory64).toBe(true);
    expect(info.wasm64).toBe(true);
    expect(info.workers).toBe(6);
    expect(s.workers).toBe(6);
    expect(info.vramBudgetMiB).toBe(3584);
    expect(s.rects).toBeGreaterThan(50); // app bar text, physics view, counter...
  });

  test('picks WebGPU when available and reserves the VRAM pool, else falls back to WebGL', async ({ page }, testInfo) => {
    const s = await bootApp(page);
    if (testInfo.project.name === 'chromium-webgpu') {
      expect(s.renderer).toBe('sdl_gpu/webgpu');
      expect(s.vramPages).toBe(14);
      expect(s.vramReserved).toBe(3584 * 1024 * 1024);
    } else {
      expect(s.renderer).toMatch(/^sdl_(gpu\/webgpu|renderer\/)/);
    }
  });

  test('Jolt physics steps on worker threads every frame', async ({ page }) => {
    const first = await bootApp(page);
    await page.waitForTimeout(1000);
    const later = await stats(page);
    expect(later.frame).toBeGreaterThan(first.frame);
    expect(later.physicsSteps).toBeGreaterThan(first.physicsSteps);
    expect(later.bodies).toBe(3); // ground + two walls
  });

  test('tapping the FAB increments the counter, spawns bodies and persists to OPFS', async ({ page }) => {
    await bootApp(page);
    const before = Number((await readOpfs(page, 'counter')) ?? '0');
    for (let i = 0; i < 3; i++) {
      await tapFab(page);
      await page.waitForTimeout(150);
    }
    await expect.poll(async () => (await stats(page)).bodies).toBe(6);
    await expect.poll(() => readOpfs(page, 'counter')).toBe(String(before + 3));

    // Reload: the counter is restored from OPFS (the physics world is not persisted).
    await page.reload();
    await bootApp(page);
    await tapFab(page);
    await expect.poll(() => readOpfs(page, 'counter')).toBe(String(before + 4));
  });

  test('tapping the physics view drops a box at the tap position', async ({ page }) => {
    await bootApp(page);
    await page.mouse.click(300, 200);
    await expect.poll(async () => (await stats(page)).bodies).toBe(4);
  });

  test('the frame is rasterized: app bar, physics view and FAB colours', async ({ page }) => {
    await bootApp(page, '/', 60);
    const [appBar, physics, fab] = await samplePixels(page, [[600, 8], [500, 300], [1024 - 44, 700 - 30]]);
    expect(closeTo(appBar, [0xff, 0x57, 0x22]), `app bar ${appBar}`).toBe(true);
    expect(closeTo(physics, [0x1e, 0x1e, 0x2e]), `physics view ${physics}`).toBe(true);
    expect(closeTo(fab, [0xff, 0x57, 0x22]), `fab ${fab}`).toBe(true);
  });
});

test.describe('Phoenix DevTools', () => {
  test('receives live stats and logs over BroadcastChannel', async ({ page, context }) => {
    const devtools = await context.newPage();
    await devtools.goto('/__phoenix/devtools/');
    await bootApp(page);
    await expect(devtools.locator('#workers')).toHaveText('6');
    await expect(devtools.locator('#status')).toHaveAttribute('aria-label', 'connected');
    await expect(devtools.locator('#logs li').first()).toBeVisible();
  });
});

test.describe('GitHub Pages hosting (no COOP/COEP headers)', () => {
  test('the service worker enables cross-origin isolation and the app boots', async ({ page }) => {
    await page.goto('http://127.0.0.1:8124/');
    // First load registers phoenix_sw.js and reloads itself.
    await page.waitForFunction(() => (globalThis as any).crossOriginIsolated === true, null, { timeout: 30_000 });
    const s = await bootApp(page, 'http://127.0.0.1:8124/');
    expect(s.workers).toBe(6);
  });
});
