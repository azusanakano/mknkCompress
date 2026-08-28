mknkCompress 2.0.0 Native
=========================

三毛猫PC堂の、JPG専用・完全オフライン画像圧縮アプリです。
Electron、Chromium、Node.js、WebViewを使用しないWindowsネイティブ版です。

使い方
------
1. mknkCompress.exe を起動します。
2. 必要に応じて画質、保存先、テーマを設定します。
3. 画像またはフォルダーを画面へドロップします。
4. ドロップ直後に自動圧縮が始まります。

通常のIntel/AMD製Windows 10/11 PCでは x64版を使用してください。
x86版は32bit Windows用、ARM64版はARM版Windows用です。

主な仕様
--------
- 出力形式はJPG固定
- 複数の画質候補を生成し、知覚品質条件を満たす最小候補を採用
- 元画像以上の容量なら保存しない設定
- 透明部分は白背景へ合成
- アニメーション画像は先頭フレームを処理
- EXIF Orientationを反映
- 反重力斥力場／重力圧縮テーマ
- 設定は %LOCALAPPDATA%\mknkCompress\settings.ini に保存

対応形式
--------
JPEG、PNG、BMP、TIFF、GIFはWindows標準機能で処理します。
WebP、AVIF、HEIC/HEIFは対応WICコーデックがWindowsに導入済みの場合に処理できます。

注意
----
- 本バイナリはコード署名されていないため、Windowsが「不明な発行元」と表示する場合があります。
- 実行にはWindows 10またはWindows 11が必要です。
- 画像や利用情報を外部へ送信する機能はありません。
