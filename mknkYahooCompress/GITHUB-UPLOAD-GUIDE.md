# GitHub登録手順

このフォルダーの内容を、そのままGitHubリポジトリのルートへ登録できます。

## Web画面から登録する場合

1. GitHubで新しい空のリポジトリを作成します。
2. README・LICENSE・`.gitignore`をGitHub側では追加せず、空のまま作成します。
3. このパッケージを展開します。
4. `mknkYahooCompress`フォルダー内の全ファイルをアップロードします。
5. コミットメッセージを `Initial release: mknkYahooCompress 3.0.0` として登録します。

## Gitコマンドで登録する場合

```bash
git init
git branch -M main
git add .
git commit -m "Initial release: mknkYahooCompress 3.0.0"
git remote add origin https://github.com/USER/REPOSITORY.git
git push -u origin main
git tag -a v3.0.0 -m "mknkYahooCompress 3.0.0"
git push origin v3.0.0
```

`USER/REPOSITORY`は実際のGitHubユーザー名とリポジトリ名へ置き換えてください。

## 登録後の確認

- Actionsで `Build mknkYahooCompress Windows Native` が成功すること
- READMEがリポジトリのトップページへ表示されること
- `dist/x64/mknkYahooCompress.exe` が登録されていること
- Releasesへ `v3.0.0` の配布ZIPとSHA-256一覧を添付すること

## 推奨リポジトリ設定

- Default branch: `main`
- Issues: 有効
- Actions: 有効
- ブランチ保護: Actions成功を必須にする
- 初回Releaseタグ: `v3.0.0`

