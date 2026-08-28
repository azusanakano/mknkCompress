# 同梱済み実行ファイルの来歴

## 対象

`release-prebuilt` に同梱するmknkCompress 2.0.0 Nativeのx64、x86、ARM64実行ファイルおよび配布ZIP。

## ビルド情報

- ツールチェーン: LLVM-MinGW 20260616 UCRT
- コンパイラ／リンカー: Clang / LLD 22.1.8
- ビルド環境: LinuxからWindows向けにクロスコンパイル
- アプリケーション実装: `src/main.cpp` のSHA-256は `4e25c9cf50aaf84b01d164c25b11b30c22c8c2337cd14e57f719c5f44ad785bb`
- コード署名: なし

## 検証済み事項

- x64: PE32+ GUI、Machine 0x8664
- x86: PE32 GUI、Machine 0x014c
- ARM64: PE32+ GUI、Machine 0xaa64
- ソース静的要件: 17/17 PASS
- アルゴリズム参照テスト: 6/6 PASS
- 配布ZIPの既存SHA-256: 全件一致

## 制限

これらの実行ファイルはWindows上でコンパイルしたMSVC版ではありません。実Windows環境での起動、WICによる画像処理、起動時間の実測は未完了です。

本リポジトリのGitHub Actionsは、Windows ServerとVisual Studio/MSVCでx64、x86、ARM64を再ビルドして別の成果物を生成する構成です。
