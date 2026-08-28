# mknkCompress 3.0.0 Linux

三毛猫PC堂の、Linux Mint向けJPG専用・完全ローカル画像圧縮アプリです。インストール不要のPortable版は、展開後の `mknkCompress` をダブルクリックして使えます。

## 対応環境

- Linux Mint 21.3 / 22.x（64-bit x86_64）
- Cinnamon / MATE / Xfce のデスクトップセッション
- GTK 3（Linux Mint標準搭載）

## 主な機能

- JPG専用出力
- 画像／フォルダーのドラッグ＆ドロップ
- ドロップ直後に現在の設定で自動圧縮
- ファイル選択ボタンから追加した場合は「圧縮開始」で実行
- mozjpeg、progressive JPEG、Trellis、最適化スキャン
- 4:4:4／4:2:0と複数品質候補の比較
- 輝度68%・色差20%・輪郭12%の知覚スコアを使い、合格候補のうち最小容量を採用
- 元画像以上の容量なら保存しない設定
- 透明部分を白背景へ合成
- アニメーション画像は先頭フレームを処理
- EXIF／ICCメタデータ保持（任意）
- 最大幅／最大高さを指定した縮小
- ファイル日時保持
- 反重力斥力場／重力圧縮テーマ、ダーク／ライト表示
- 設定を `~/.config/mknkCompress/settings.ini` へ保存

## 使い方

1. Portable版を任意の場所へ展開します。
2. `mknkCompress` をダブルクリックします。実行確認が出た場合は「実行」を選びます。
3. 画像またはフォルダーを画面へドロップします。現在の設定で自動的に圧縮が始まります。

コマンドラインからは次のように起動できます。

```bash
./mknkCompress
```

画像パスを引数にすると、追加後すぐに圧縮します。

```bash
./mknkCompress "/path/to/photo.png"
```

## プライバシー

画像、ファイル名、設定、利用情報を外部へ送信しません。ネットワークAPIは使用していません。画像処理は同梱のNode.jsとSharp/libvipsでローカル実行します。

## ソースからビルド

GTK 3ランタイム、GCC 13以降を用意し、次を実行します。

```bash
./build-linux.sh
```

GUI実行ファイルは `build/mknkCompress` に作成されます。Portable版の作成には、Node.js 20以降とSharp 0.35.3のLinux x64モジュールが必要です。
