import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import sharp from 'sharp';

const root = path.resolve(path.dirname(new URL(import.meta.url).pathname), '..');
const backend = path.join(root, 'app/compress.mjs');
const temp = await fs.mkdtemp(path.join(os.tmpdir(), 'mknk-test-'));
try {
  const input = path.join(temp, '透明テスト.png');
  const output = path.join(temp, '結果.jpg');
  const raw = Buffer.alloc(640 * 480 * 4);
  for (let y = 0; y < 480; y++) for (let x = 0; x < 640; x++) {
    const p = (y * 640 + x) * 4;
    raw[p] = x % 256; raw[p + 1] = y % 256; raw[p + 2] = (x + y) % 256; raw[p + 3] = x < 100 ? 0 : 255;
  }
  await sharp(raw, { raw: { width: 640, height: 480, channels: 4 } }).png().toFile(input);
  const result = spawnSync(process.execPath, [backend, '--input', input, '--output', output, '--quality', '82', '--resize', '1', '--width', '320', '--height', '240', '--metadata', '0', '--smaller', '0', '--timestamps', '1'], { encoding: 'utf8' });
  assert.equal(result.status, 0, result.stderr + result.stdout);
  assert.match(result.stdout, /^OK\t/m);
  const metadata = await sharp(output).metadata();
  assert.equal(metadata.format, 'jpeg');
  assert.equal(metadata.width, 320);
  assert.equal(metadata.height, 240);
  assert.equal(metadata.channels, 3);
  console.log('PASS PNG→JPG、透明白背景、縮小、Unicodeパス');

  const tiny = path.join(temp, 'tiny.jpg');
  const skipped = path.join(temp, 'tiny-out.jpg');
  await sharp({ create: { width: 8, height: 8, channels: 3, background: 'white' } }).jpeg({ quality: 70 }).toFile(tiny);
  const skipResult = spawnSync(process.execPath, [backend, '--input', tiny, '--output', skipped, '--quality', '96', '--resize', '0', '--width', '1920', '--height', '1080', '--metadata', '0', '--smaller', '1', '--timestamps', '1'], { encoding: 'utf8' });
  assert.equal(skipResult.status, 0, skipResult.stderr + skipResult.stdout);
  assert.match(skipResult.stdout, /^SKIP\t/m);
  await assert.rejects(fs.stat(skipped));
  console.log('PASS 元画像以上なら保存しない');
} finally {
  await fs.rm(temp, { recursive: true, force: true });
}
