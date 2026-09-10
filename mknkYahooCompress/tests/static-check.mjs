import fs from 'node:fs';
import assert from 'node:assert/strict';

const main = fs.readFileSync(new URL('../src/main.cpp', import.meta.url), 'utf8');
const cmake = fs.readFileSync(new URL('../CMakeLists.txt', import.meta.url), 'utf8');
const readme = fs.readFileSync(new URL('../README.md', import.meta.url), 'utf8');
const versionResource = fs.readFileSync(new URL('../resources/mknkCompress.rc', import.meta.url), 'utf8');
const manifest = fs.readFileSync(new URL('../resources/app.manifest', import.meta.url), 'utf8');
const license = fs.readFileSync(new URL('../LICENSE', import.meta.url), 'utf8');
const source = main + cmake + readme + versionResource + manifest + license;

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
  ['元容量と5MBの小さい方を上限化', /effectiveTargetBytes=std::min\(kYahooTargetBytes,item->inputSize\?item->inputSize:kYahooTargetBytes\)/],
  ['候補選択で実効上限を使用', /it->size>effectiveTargetBytes/],
  ['保存後の実効上限再検証', /savedSize>effectiveTargetBytes/],
  ['テーマ2種', /反重力斥力場/],
  ['重力テーマ', /重力圧縮/],
  ['設定永続化', /WritePrivateProfileStringW/],
  ['接尾辞の永続化', /WritePrivateProfileStringW\(L"output", L"suffix"/],
  ['接尾辞の保存ボタン', /IDC_SUFFIX_SAVE/],
  ['接尾辞の説明表示', /圧縮後の接尾辞/],
  ['接尾辞の禁止文字検査', /IsValidSuffix[\s\S]*find_first_of/],
  ['不正な保存値の初期化', /LoadSettings[\s\S]*!IsValidSuffix\(g_settings\.suffix\)[\s\S]*L"_yahoo"/],
  ['接尾辞の64文字制限', /EM_SETLIMITTEXT,64/],
  ['圧縮前の接尾辞検証', /StartCompression[\s\S]*CollectSettings\(hwnd,true\)/],
  ['終了時の接尾辞保存', /WM_CLOSE[\s\S]*CollectSettings\(hwnd,true\)/],
  ['ドラッグ＆ドロップ', /WM_DROPFILES/],
  ['ドロップ後の自動圧縮', /WM_DROPFILES[\s\S]*StartCompression\(hwnd\)/],
  ['x64 x86 ARM64', /x64.*x86.*ARM64/s],
  ['静的Cランタイム', /MultiThreaded/],
  ['ネコシステム社UIブランド', /ネコシステム社\s+•\s+3\.0\.4/],
  ['EXE会社名ブランド', /VALUE "CompanyName", "ネコシステム社\\0"/],
  ['マニフェストブランドID', /name="NekoSystem\.mknkYahooCompress"/],
];
for (const [name, pattern] of checks) assert.match(source, pattern, name);
assert.doesNotMatch(cmake, /electron|node|webview/i);
assert.doesNotMatch(source, /三毛猫PC堂|MikenekoPC/, '旧ブランド表記が残っていない');
console.log(`${checks.length + 2} checks passed`);
