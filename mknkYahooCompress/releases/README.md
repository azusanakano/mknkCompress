# mknkYahooCompress 3.0.4 最新修正版

最新の配布物は、ファイル名に **`3.0.4-review-fix`** を含む次のZIPです。
アプリを終了し、ご使用のWindowsに合うZIPを展開してEXEを入れ替えてください。

| 用途 | ダウンロード |
|---|---|
| Windows x64 | [mknkYahooCompress-3.0.4-review-fix-Windows-x64.zip](mknkYahooCompress-3.0.4-review-fix-Windows-x64.zip) |
| Windows x86 | [mknkYahooCompress-3.0.4-review-fix-Windows-x86.zip](mknkYahooCompress-3.0.4-review-fix-Windows-x86.zip) |
| Windows ARM64 | [mknkYahooCompress-3.0.4-review-fix-Windows-arm64.zip](mknkYahooCompress-3.0.4-review-fix-Windows-arm64.zip) |
| 3種類のWindows版をまとめて取得 | [mknkYahooCompress-3.0.4-review-fix-All-Architectures.zip](mknkYahooCompress-3.0.4-review-fix-All-Architectures.zip) |
| 修正版ソース | [mknkYahooCompress-3.0.4-review-fix-native-source.zip](mknkYahooCompress-3.0.4-review-fix-native-source.zip) |

`Native` および `queue-fix` を含む従来のZIPは比較・記録用の旧配布物です。
同じバージョン表記の3.0.4でも今回の修正を含まないため、新規利用・更新には上記の `review-fix` 版を使用してください。

今回の修正版では、元画像を差し替えて再試行するときの容量判定と、処理中に追加できなかった場合の画面表示を修正しています。
完了済み画像の結果を保持し、追加した未完了画像だけを処理します。
処理中の追加は受け付けないため、完了またはキャンセル後にもう一度追加してください。

JPG固定、5,000,000 bytes以下、元画像の容量以下、長辺1200px以下の仕様と、接尾辞・出力先・テーマ・ウィンドウ配置の保存は引き続き利用できます。
検証範囲と未実施項目は、各ZIP内の `BUILD-REPORT.txt` に記載しています。
