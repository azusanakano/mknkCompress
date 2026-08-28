import fs from 'node:fs';
import path from 'node:path';
import process from 'node:process';

const root = path.resolve(path.dirname(new URL(import.meta.url).pathname), '..');
const gui = fs.readFileSync(path.join(root, 'src/main.cpp'), 'utf8');
const backend = fs.readFileSync(path.join(root, 'app/compress.mjs'), 'utf8');
const checks = [
  ['JPG専用UI', gui.includes('JPG専用')],
  ['ドラッグ＆ドロップ', gui.includes('drag-data-received') && gui.includes('StartProcessing()')],
  ['自動開始', gui.includes('if (added) StartProcessing();')],
  ['元画像以上なら保存しない', backend.includes('selected.buffer.length >= inputStat.size')],
  ['白背景化', backend.includes('.flatten(')],
  ['先頭フレーム', backend.includes('page: 0, pages: 1')],
  ['mozjpeg', backend.includes('mozjpeg: true')],
  ['progressive JPEG', backend.includes('progressive: true')],
  ['Trellis', backend.includes('trellisQuantisation: true')],
  ['4:4:4/4:2:0比較', backend.includes("['4:2:0', '4:4:4']")],
  ['知覚評価', backend.includes('perceptualScore')],
  ['メタデータ保持', backend.includes('keepMetadata')],
  ['タイムスタンプ保持', backend.includes('fs.utimes')],
  ['テーマ切替', gui.includes('反重力斥力場') && gui.includes('重力圧縮')],
  ['完全ローカル', !/https?:\/\//.test(backend)],
];
let failed = 0;
for (const [name, ok] of checks) {
  console.log(`${ok ? 'PASS' : 'FAIL'} ${name}`);
  if (!ok) failed++;
}
console.log(`${checks.length - failed}/${checks.length} checks passed`);
process.exitCode = failed ? 1 : 0;
