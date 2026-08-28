import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import process from 'node:process';
import sharp from 'sharp';

const argv = new Map();
for (let i = 2; i < process.argv.length; i += 2) argv.set(process.argv[i], process.argv[i + 1] ?? '');

function cleanMessage(error) {
  return String(error?.message ?? error).replace(/[\t\r\n]+/g, ' ').slice(0, 300);
}

function intArg(name, fallback, low, high) {
  const value = Number.parseInt(argv.get(name) ?? '', 10);
  return Number.isFinite(value) ? Math.max(low, Math.min(high, value)) : fallback;
}

function pipeline(input, settings) {
  let image = sharp(input, { animated: false, page: 0, pages: 1, failOn: 'warning' })
    .rotate()
    .flatten({ background: { r: 255, g: 255, b: 255 } });
  if (settings.resize) {
    image = image.resize({
      width: settings.width,
      height: settings.height,
      fit: 'inside',
      withoutEnlargement: true,
      kernel: sharp.kernel.lanczos3
    });
  }
  return image;
}

function jpegOptions(quality, chromaSubsampling, metadata) {
  return {
    quality,
    chromaSubsampling,
    progressive: true,
    mozjpeg: true,
    optimiseCoding: true,
    optimiseScans: true,
    trellisQuantisation: true,
    overshootDeringing: true,
    quantisationTable: quality >= 86 ? 0 : 3,
    force: true
  };
}

async function rawSample(source) {
  const { data, info } = await sharp(source)
    .resize({ width: 320, height: 320, fit: 'inside', withoutEnlargement: true })
    .removeAlpha()
    .toColourspace('srgb')
    .raw()
    .toBuffer({ resolveWithObject: true });
  return { data, width: info.width, height: info.height, channels: info.channels };
}

function perceptualScore(reference, candidate) {
  if (reference.width !== candidate.width || reference.height !== candidate.height) return 0;
  const a = reference.data;
  const b = candidate.data;
  const pixels = reference.width * reference.height;
  let lumaError = 0;
  let chromaError = 0;
  let edgeError = 0;
  const lumasA = new Float32Array(pixels);
  const lumasB = new Float32Array(pixels);
  for (let p = 0; p < pixels; p++) {
    const ai = p * reference.channels;
    const bi = p * candidate.channels;
    const ar = a[ai], ag = a[ai + 1], ab = a[ai + 2];
    const br = b[bi], bg = b[bi + 1], bb = b[bi + 2];
    const ya = 0.2126 * ar + 0.7152 * ag + 0.0722 * ab;
    const yb = 0.2126 * br + 0.7152 * bg + 0.0722 * bb;
    lumasA[p] = ya;
    lumasB[p] = yb;
    lumaError += Math.abs(ya - yb) / 255;
    chromaError += (Math.abs((ar - ag) - (br - bg)) + Math.abs((ab - ag) - (bb - bg))) / 1020;
  }
  const width = reference.width;
  for (let y = 1; y < reference.height; y++) {
    for (let x = 1; x < width; x++) {
      const p = y * width + x;
      const edgeA = Math.abs(lumasA[p] - lumasA[p - 1]) + Math.abs(lumasA[p] - lumasA[p - width]);
      const edgeB = Math.abs(lumasB[p] - lumasB[p - 1]) + Math.abs(lumasB[p] - lumasB[p - width]);
      edgeError += Math.abs(edgeA - edgeB) / 510;
    }
  }
  const edgePixels = Math.max(1, (reference.width - 1) * (reference.height - 1));
  const weighted = 0.68 * (lumaError / pixels) + 0.20 * (chromaError / pixels) + 0.12 * (edgeError / edgePixels);
  return Math.max(0, Math.min(100, 100 * (1 - weighted)));
}

async function compress(input, output, settings) {
  const inputStat = await fs.stat(input);
  const referenceBuffer = await pipeline(input, settings).png().toBuffer();
  const reference = await rawSample(referenceBuffer);
  const qualities = [...new Set([
    settings.quality - 12,
    settings.quality - 8,
    settings.quality - 4,
    settings.quality,
    settings.quality + 3
  ].map(q => Math.max(35, Math.min(96, q))))];
  const candidates = [];
  for (const quality of qualities) {
    for (const chroma of ['4:2:0', '4:4:4']) {
      let image = pipeline(input, settings);
      if (settings.metadata) image = image.keepMetadata();
      const buffer = await image.jpeg(jpegOptions(quality, chroma, settings.metadata)).toBuffer();
      const decoded = await rawSample(buffer);
      const score = perceptualScore(reference, decoded);
      candidates.push({ buffer, quality, chroma, score });
    }
  }
  const threshold = Math.min(99.2, 84 + settings.quality * 0.12);
  const passing = candidates.filter(candidate => candidate.score >= threshold);
  const pool = passing.length ? passing : candidates;
  pool.sort((a, b) => a.buffer.length - b.buffer.length || b.score - a.score);
  const selected = pool[0];
  if (settings.onlyIfSmaller && selected.buffer.length >= inputStat.size) {
    return { skipped: true, reason: '元画像以上の容量' };
  }
  await fs.mkdir(path.dirname(output), { recursive: true });
  const temporary = path.join(path.dirname(output), `.${path.basename(output)}.${process.pid}.tmp`);
  try {
    await fs.writeFile(temporary, selected.buffer, { mode: 0o644 });
    await fs.rename(temporary, output);
  } catch (error) {
    await fs.rm(temporary, { force: true }).catch(() => {});
    throw error;
  }
  if (settings.timestamps) await fs.utimes(output, inputStat.atime, inputStat.mtime);
  return {
    skipped: false,
    size: selected.buffer.length,
    quality: selected.quality,
    candidates: candidates.length,
    score: selected.score,
    output
  };
}

async function main() {
  if (process.argv.includes('--self-test')) {
    const formats = ['jpeg', 'png', 'webp', 'tiff', 'gif', 'svg', 'heif'].filter(name => sharp.format[name]?.input?.buffer);
    console.log(`BACKEND SELF-TEST PASS sharp=${sharp.versions.sharp} vips=${sharp.versions.vips} formats=${formats.join(',')}`);
    return;
  }
  const input = argv.get('--input');
  const output = argv.get('--output');
  if (!input || !output) throw new Error('入力または出力パスがありません');
  const settings = {
    quality: intArg('--quality', 82, 35, 96),
    resize: argv.get('--resize') === '1',
    width: intArg('--width', 1920, 1, 50000),
    height: intArg('--height', 1080, 1, 50000),
    metadata: argv.get('--metadata') === '1',
    onlyIfSmaller: argv.get('--smaller') !== '0',
    timestamps: argv.get('--timestamps') !== '0'
  };
  const result = await compress(input, output, settings);
  if (result.skipped) console.log(`SKIP\t${result.reason}`);
  else console.log(`OK\t${result.size}\t${result.quality}\t${result.candidates}\t${result.score.toFixed(3)}\t${result.output}`);
}

main().catch(error => {
  console.log(`ERROR\t${cleanMessage(error)}`);
  process.exitCode = 1;
});
