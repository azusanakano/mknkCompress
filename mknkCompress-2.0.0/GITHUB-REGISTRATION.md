# GitHub登録手順

このフォルダーは、そのまま `mknkCompress` リポジトリのルートとして登録できます。

## 推奨設定

- リポジトリ名: `mknkCompress`
- 公開範囲: Private
- 既定ブランチ: `main`
- ライセンス: 同梱の `LICENSE`（MIT）
- GitHub側でREADME、.gitignore、LICENSEを追加生成しない

## Gitで登録する場合

```bash
git init -b main
git add .
git commit -m "Import mknkCompress 2.0.0 Native"
git remote add origin https://github.com/azusanakano/mknkCompress.git
git push -u origin main
```

## Windows/MSVC版を生成する場合

GitHubのActions画面から `Build Windows Native` を手動実行できます。次のようにタグを登録してもWindowsビルドが始まります。

```bash
git tag v2.0.0
git push origin v2.0.0
```

生成物はActionsの `mknkCompress-2.0.0-Native-MSVC-release` アーティファクトから取得します。

## 同梱済み実行ファイル

- `release-prebuilt/x64/mknkCompress.exe`: 一般的な64bit Intel/AMD Windows用
- `release-prebuilt/x86/mknkCompress.exe`: 32bit Windows用
- `release-prebuilt/arm64/mknkCompress.exe`: ARM版Windows用
- `release-prebuilt/packages/`: 各アーキテクチャの配布ZIP

同梱済み実行ファイルはLLVM-MinGWによるWindows向けクロスビルド版です。Windows上での実行確認は未実施です。詳細は `BUILD-REPORT.txt` と `PREBUILT-BINARY-PROVENANCE.md` を参照してください。
