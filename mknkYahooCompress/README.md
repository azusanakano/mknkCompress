# mknkYahooCompress 3.0.0 Native

三毛猫PC堂の、Yahoo!オークション（ヤフオク）出品画像専用Windowsネイティブ圧縮アプリです。
Electron・Node.js・Chromium・WebViewを使わず、Win32/C++とWindows Imaging Component（WIC）だけで動作します。画像や利用情報を外部へ送信しません。

## ヤフオク専用の固定仕様

- 出力形式：JPG固定
- 1枚の出力容量：5,000,000 bytes以下
- 画像寸法：縦横比を維持し、長辺1200px以下
- 出力名：元ファイル名 + `_yahoo.jpg`
- ドラッグ＆ドロップ直後に自動処理

2026年8月28日に確認したYahoo!オークション公式ヘルプでは、商品画像は1枚5MB以内、JPEGまたはGIF、最大10枚です。幅または高さが1200pxを超える画像は掲載時に縮小されます。本アプリはこの公式仕様へ合わせます。

公式ヘルプ：https://support.yahoo-net.jp/PccAuctions/s/article/H000005298

## 画質優先アルゴリズム

1. EXIF Orientationを反映します。
2. 長辺が1200pxを超える場合だけWIC Fant補間で縮小します。
3. JPEG品質100から35まで、4:4:4と4:2:0の候補を生成します。
4. 320pxサンプルで輝度68%、色差20%、輪郭12%の知覚スコアを比較します。
5. 5,000,000 bytes以内の候補から、知覚品質が最も高いものを採用します。
6. 保存後にも5MB上限を再検証し、超過時は出力を残しません。

容量を最小化するのではなく、ヤフオクで受理される容量内で画質を最大化する設計です。

## 主な機能

- JPG／JPEG／PNG／BMP／TIFF／GIF入力
- WebP／AVIF／HEIC／HEIF入力（対応WICコーデック導入時）
- 画像またはフォルダーのドラッグ＆ドロップ
- 透明部分を白背景へ合成
- アニメーション画像は先頭フレームを処理
- 撮影情報・ICC保持の任意切替（既定はプライバシー優先でOFF）
- 元画像の更新日時を出力へ継承
- 反重力斥力場／重力圧縮テーマ、ダーク／ライト表示
- x64／x86／ARM64ビルド構成

## 対応OS

Windows 10 / 11。

## 使い方

1. `mknkYahooCompress.exe`を起動します。
2. 画像またはフォルダーを画面へドロップします。
3. 自動でヤフオク用JPGが作成されます。

出力先を空欄にすると元画像と同じフォルダーへ保存します。元ファイルは上書きしません。

## ビルド

Visual Studio 2022またはBuild Tools 2022へ「C++によるデスクトップ開発」、Windows 10/11 SDK、CMakeを導入し、`build-windows.cmd`を実行してください。x64／x86／ARM64を順にビルドします。

PowerShellで配布ZIPまで作る場合：

```powershell
./package-release.ps1
```

LinuxからLLVM-MinGWでクロスコンパイルする場合：

```bash
./build-cross-linux.sh /path/to/llvm-mingw
```

Cランタイムは静的リンクするため、VC++再頒布可能パッケージは不要です。

## プライバシー

ネットワークAPIは使用しません。設定は `%LOCALAPPDATA%\\mknkYahooCompress\\settings.ini` に保存します。候補画像はWindowsの一時フォルダーで生成し、処理後に削除します。
