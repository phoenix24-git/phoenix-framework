// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
import { expect, type Page } from '@playwright/test';

export interface EngineStats {
  fps: number;
  frame: number;
  bodies: number;
  physicsSteps: number;
  workers: number;
  vramReserved: number;
  vramPages: number;
  rects: number;
  renderer: string;
  storage: string;
}

/** Loads the app and waits until the engine has rendered `frames` frames. */
export async function bootApp(page: Page, url = '/', frames = 30): Promise<EngineStats> {
  const errors: string[] = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto(url);
  await page.waitForFunction(
    (n) => (globalThis as any).phoenix?.status === 'error' || ((globalThis as any).phoenix?.stats?.frame ?? 0) >= n,
    frames,
    { timeout: 60_000 },
  );
  const error = await page.evaluate(() => (globalThis as any).phoenix?.error);
  expect(error, `engine error; page errors: ${errors.join('\n')}`).toBeUndefined();
  return stats(page);
}

export function stats(page: Page): Promise<EngineStats> {
  return page.evaluate(() => (globalThis as any).phoenix.stats);
}

/** Clicks the FloatingActionButton (56px, 16px from the bottom-right corner). */
export async function tapFab(page: Page): Promise<void> {
  const vp = page.viewportSize()!;
  await page.mouse.click(vp.width - 16 - 28, vp.height - 16 - 28);
}

/** Reads a file written by phoenix::storage from the Origin Private File System. */
export function readOpfs(page: Page, name: string): Promise<string | null> {
  return page.evaluate(async (file) => {
    try {
      const root = await navigator.storage.getDirectory();
      const handle = await root.getFileHandle(file);
      return await (await handle.getFile()).text();
    } catch {
      return null;
    }
  }, name);
}

/** Returns the [r, g, b] of the given points from a screenshot (decoded in the browser). */
export async function samplePixels(page: Page, points: Array<[number, number]>): Promise<number[][]> {
  const png = (await page.screenshot()).toString('base64');
  return page.evaluate(
    async ({ data, pts }) => {
      const img = new Image();
      img.src = 'data:image/png;base64,' + data;
      await img.decode();
      const canvas = new OffscreenCanvas(img.width, img.height);
      const ctx = canvas.getContext('2d')!;
      ctx.drawImage(img, 0, 0);
      return pts.map(([x, y]) => Array.from(ctx.getImageData(x, y, 1, 1).data.slice(0, 3)));
    },
    { data: png, pts: points },
  );
}

export function closeTo(actual: number[], expected: number[], tolerance = 12): boolean {
  return actual.every((v, i) => Math.abs(v - expected[i]) <= tolerance);
}
