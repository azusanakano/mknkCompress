# mknkCompress 2.0.0 Native

三毛猫PC堂の、起動速度を優先したWindowsネイティブJPG圧縮アプリです。
Electron・Node.js・Chromium・WebViewを使用せず、Win32/C++とWindows Imaging Component（WIC）だけで動作します。画像や利用情報を外部へ送信しません。

## 主な機能

- JPG専用出力
- 画像／フォルダーのドラッグ＆ドロップ
- ドロップ直後に現在の設定で自動圧縮
- 品質候補を複数生成し、知覚スコアを満たす最小候補を採用
- 元画像以上の容量なら保存しない設定
- 透明部分を白背景へ合成
- アニメーション画像は先頭フレームを処理
- EXIF・ICC等のメタデータをWICが対応可能な範囲で保持
- EXIF Orientationを反映
- 最大幅／最大高さを指定した縮小
- タイムスタンプ保持
- 反重力斥力場／重力圧縮テーマ、ダーク／ライト表示
- 設定を次回起動時に復元
- x64／x86／ARM64のビルド構成

## 対応OS

Windows 10 / 11。JPEG、PNG、BMP、TIFF、GIFはWindows標準WICで処理できます。WebP、AVIF、HEIC/HEIFは、その形式のWICコーデックがWindowsへ導入済みの場合に処理できます。

## 旧Electron版からの変更

起動時にChromiumを展開・初期化しないため、起動負荷と配布容量を抑えています。圧縮基盤はSharp/mozjpegからWindows標準WICへ変更しました。このため、同じ品質値でも旧版とファイルサイズや画質は完全には一致しません。

次の選定ロジックは1.4.0から移植しています。

1. 指定画質を基点に複数の画質・色差サンプリング候補を生成
2. 320pxサンプルで輝度68%、色差20%、輪郭12%の知覚誤差を算出
3. 指定画質に応じた知覚スコア下限を適用
4. 合格候補のうち最小容量を採用
5. 「小さくなる場合だけ保存」が有効なら肥大化結果を破棄

WIC JPEGエンコーダはmozjpegのTrellis／最適化スキャンと同一ではありません。これは不具合ではなく、追加ランタイムを廃してWindows標準機能だけで高速起動するための設計上の差です。

## ビルド

必要なもの：Visual Studio 2022 または Build Tools 2022 の「C++によるデスクトップ開発」、Windows 10/11 SDK、CMakeコンポーネント。

`build-windows.cmd` を実行すると、x64／x86／ARM64を順にビルドします。配布ZIPまで作る場合はPowerShellで次を実行します。

```powershell
./package-release.ps1
```

生成先は `release` フォルダーです。Cランタイムは静的リンクするため、VC++再頒布可能パッケージは不要です。

GitHub登録用パッケージには、検証済みの既存バイナリを `release-prebuilt` に同梱しています。通常のIntel/AMD製Windows 10/11では `release-prebuilt/x64/mknkCompress.exe` を使用してください。これらはLLVM-MinGWによるクロスビルド版です。GitHub Actionsを手動実行するか `v*` タグをpushすると、Windows Server上のVisual Studio/MSVCによる別パッケージが生成されます。

Linuxから公式LLVM-MinGWでクロスコンパイルする場合は、ツールチェーンを展開して次を実行します。

```bash
./build-cross-linux.sh /path/to/llvm-mingw
```

配布済みの2.0.0 NativeバイナリはLLVM-MinGW 20260616（Clang 22.1.8、UCRT）で再現可能ビルドされています。

## プライバシー

ネットワークAPIは使用していません。設定は `%LOCALAPPDATA%\mknkCompress\settings.ini` に保存します。画像は指定先または元画像と同じフォルダーへ保存します。候補画像はWindowsの一時フォルダーで生成し、採用・中止・失敗後に削除します。
