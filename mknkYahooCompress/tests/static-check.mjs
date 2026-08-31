import fs from 'node:fs';
import assert from 'node:assert/strict';

const main = fs.readFileSync(new URL('../src/main.cpp', import.meta.url), 'utf8');
const cmake = fs.readFileSync(new URL('../CMakeLists.txt', import.meta.url), 'utf8');
const readme = fs.readFileSync(new URL('../README.md', import.meta.url), 'utf8');

const checks = [
  ['Win32エントリポイント', /wWinMain/],
  ['Electron非依存', /CoCreateInstance\(CLSID_WICImagingFactory/],
  ['JPGエンコーダ', /GUID_ContainerFormatJpeg/],
  ['白背景合成', /255u \* \(255u - a\)/],
  ['先頭フレーム', /GetFrame\(0/],
  ['Orientation', /ushort=274/],
  ['知覚スコア', /PerceptualScore/],
  ['ヤフオク5MB上限', /kYahooTargetBytes = 5'000'000ull/],
  ['長辺1200px', /kYahooMaxEdge = 1200/],
  ['容量内最高知覚品質', /it->score>winner->score/],
  ['保存後の上限再検証', /savedSize>kYahooTargetBytes/],
  ['テーマ2種', /反重力斥力場/],
  ['重力テーマ', /重力圧縮/],
  ['設定永続化', /WritePrivateProfileStringW/],
  ['ドラッグ＆ドロップ', /WM_DROPFILES/],
  ['ドロップ後の自動圧縮', /WM_DROPFILES[\s\S]*StartCompression\(hwnd\)/],
  ['x64 x86 ARM64', /x64.*x86.*ARM64/s],
  ['静的Cランタイム', /MultiThreaded/],
];
for (const [name, pattern] of checks) assert.match(main + cmake + readme, pattern, name);
assert.doesNotMatch(cmake, /electron|node|webview/i);
console.log(`${checks.length + 1} checks passed`);
