# Mac向け移植・配布

## 方針と対象

2026-11-01: 初期対象をApple Silicon（arm64）とする。Intel・Universal版は今回の対象に含めない。既存C++はUEのプラットフォーム抽象化を使用し、地図は同梱3D Tiles、M4AはMacのAvfMediaで再生する。Windowsのゲーム内容・パッケージ手順は維持する。

Mac向け設定を `Config/Mac/MacEngine.ini` に分離し、Metal SM5、従来メッシュ描画、レイトレーシングなしの構成にする。都市描画はCesiumの通常メッシュを使う。Metal SM6・Naniteを必須にしない構成でまず検証する。ローカル検証用にXcodeのSign to Run Locallyを使用し、バンドルIDを `io.github.fujinochan.IkebukuroNinjaCity` に固定する。公開配布のDeveloper ID署名時はこのローカル署名を置き換える。対応macOSの下限はインストール済みUE 5.8のSDK要件と生成されたInfo.plistを確認して決める。未検証のOSバージョンはサポート表記しない。

## 前提

- インストール済みMac用UE 5.8、対応するフル版Xcode、Python 3。
- Xcodeの別コンポーネントであるMetal Toolchain。`xcrun metal --version` が成功すること。未導入の場合、許可を得て `xcodebuild -downloadComponent MetalToolchain -exportPath "$PWD/Downloads/MetalToolchain"` で取得し、`xcodebuild -importComponent MetalToolchain -importPath <取得したバンドル>` で標準登録する。初回Cookが長時間待機する前にpackage.shが検出する。
- Cesium for Unreal 2.30.0をプロジェクトの `Plugins/` に復元する。既存の公式ZIPを `Downloads/CesiumForUnreal-58-v2.30.0.zip` に置き `python3 Tools/setup_project.py` を実行する。固定SHA-256で検証して展開する。ZIPがない場合のネット取得は別途ユーザーの許可範囲を確認する。
- 公開配布にはApple Developer ID Application証明書と公証用資格情報を用意する。秘密情報をリポジトリに保存しない。

## ビルドと同梱検証

```sh
export UE_ENGINE_ROOT="/Users/Shared/Epic Games/UE_5.8"
bash Tools/Mac/build.sh
bash Tools/Mac/package.sh
```

`package.sh` は依存データを検証し、UATのBuildCookRunでMac arm64 ShippingをBuild/Cook/Stage/Archiveする。出力は `SavedBuilds/Mac/Package/`、ログは `Saved/Logs/Mac-Package.log` と `Saved/Logs/Mac-UAT/`。既存の出力がある場合は停止する。再ビルド前に古い出力をプロジェクト内の別フォルダへ移動する。

終了時に `verify_package.py` が起動可能権限、Info.plist、IoStore、地図・音楽の全マニフェスト項目のSHA-256、説明・出典を確認し、`Saved/Validation/mac-staging.json` に保存する。単独でも再実行できる。

```sh
python3 Tools/Mac/verify_package.py
bash Tools/Mac/archive.sh local
```

ローカル確認用ZIPは `SavedBuilds/Mac/IkebukuroNinjaCity-1.0.0-Mac-arm64-local.zip`。dittoでバンドル属性と実行権限を保持し、UAT出力全体を圧縮する。`.app` だけを取り出すとNon-UFSデータが欠ける可能性がある。SHA-256を隣に生成する。このZIPの生成は署名・公証・実機動作の合格を意味しない。

## 実機確認

ZIPをプロジェクト内の空白・日本語を含む別フォルダへ展開し、Finderからアプリを起動する。Editor、元のContent、起動ディレクトリに依存しないことを確かめる。ネットワークを切った状態でも確認する。

1. 建物、道路、地面の色、忍者、影・マテリアルの表示。
2. Enterで開始、WASD・マウス・Shift・3段ジャンプ・手裏剣・路面復帰。
3. 赤忍者の移動・3方向攻撃・踏みつけ・一撃敗北・全滅勝利。
4. BGMの実音声、ポーズ・再開、曲末敗北、リトライで先頭へ復帰、爆発効果音。
5. Qで終了、再起動、継続プレイ時の性能とメモリ。

配布アプリの地図・BGM実再生を既存のC++検証で確認する補助コマンドも用意した。`--app` に実際のアプリパスを指定する。全曲・ポーズ・再試合を含むため数分かかり、660秒でタイムアウトする。`Saved/Validation/mac-runtime-music.json` でCook済みデータ・実音声デコード・地図2系統と移動先Contentの使用を確認する。既存レポートは上書きしない。

```sh
python3 Tools/Mac/check_runtime.py --app SavedBuilds/Mac/Package/IkebukuroNinjaCity-Mac-Shipping.app
```

既存のC++自動検証は開発時の補助に使えるが、Metal描画・AVFoundation音声・Finder起動の確認を代替しない。

## 署名と公証

公開配布時は完成したアプリ内のMach-O実行ファイル・dylib・フレームワーク・ヘルパーバンドルを内側から外側の順にDeveloper ID Applicationで署名し、最後にアプリ全体を署名する。Hardened Runtimeとセキュアタイムスタンプを有効にする。`codesign --deep` を署名の代用にしない。必要なentitlementsは実ビルドで確認して最小限に設定する。署名後にバンドル内部を変更しない。

署名済みアプリでローカルZIPを作成してから、利用者が用意したキーチェーンのプロファイルで公証する。以下の `APP` はUAT出力内で実際に生成されたアプリへの絶対パスに置き換える。公証はAppleへのアップロードなので、実行にはユーザーの明示許可が必要。

```sh
xcrun notarytool submit SavedBuilds/Mac/IkebukuroNinjaCity-1.0.0-Mac-arm64-local.zip --keychain-profile IkebukuroNotary --wait
xcrun stapler staple "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"
spctl --assess --type execute --verbose=2 "$APP"
xcrun stapler validate "$APP"
bash Tools/Mac/archive.sh signed
```

`archive.sh signed` は署名・Gatekeeper評価・公証チケットを検証してから配布ZIPを生成する。`local` ZIPとは別名になる。生成した署名済みZIPを別Macへブラウザ経由で取得し、通常のGatekeeper判定で起動できることを確認する。検証失敗時にquarantine解除やGatekeeper無効化で回避しない。

## 実装時点の検証状況

UE 5.8.4、Xcode 27.2、Apple M5、macOS 26.6でEditorとarm64 ShippingのBuild/Cook/Package/Archiveが成功。地図・BGM48ファイルと配布表記の全ファイルを照合済み。初回アプリでは画面表示をユーザーが確認。日本語・空白を含む別フォルダからLaunch Servicesで起動し、BGM全曲とライフサイクル12項目、敵の射撃14項目、入力・試合進行25項目が合格。Developer ID証明書は未設定で、公開向け署名・公証は未実施。生成Info.plistの最小OSは14.00だが、macOS 14実機の動作は未検証。

## CesiumのMacコンパイラ互換修正

Xcode 27.2でCesium 2.30.0の `IonQuickAddPanel.cpp` が `-Werror,-Wuninitialized` で停止した。モーダルウィンドウを自身の初期化式内のラムダから参照していたため、ウィンドウを生成した後にSetContentする処理へ分離した。警告全体の無効化は行わない。`Tools/Mac/patch_cesium.py` をbuild/packageから実行し、固定バージョン・既知のソース形状だけに適用する。原本は `Backups/Mac-Cesium-2.30.0/` に保持する。元ソースのSHA-256は `32cd789165b1a868421d0a588c46b57845d811995372ee7028e371c4f4c5d490`。プラグイン更新時には修正の要否を再確認する。

## 保存済み地図パスの移植

Cook時にマップ内に保存されていたWindows絶対URLがCesiumに先行読み込みされ、404エラーになった。`ALocalCityTileset::PostLoad` で現在のContentルートからURLを再設定し、OnConstructionでもCesiumの親処理より前に設定する。BeginPlay時の再設定も保持し、配布アプリを移動しても現在の配置から地図を解決する。

## 配布権限と起動経路

Mac App Store向けではない自己完結アプリとして、`Build/Mac/Resources/Ikebukuro.entitlements` を明示する。UE既定のShipping用App Sandboxはプロジェクト内への検証レポート出力を拒否したため使用しない。デバッグ接続権限も付与しない。公開時は別途Hardened Runtime・Developer ID署名・公証を行う。実行チェックはLaunch Services（open -W -n）経由で起動する。

追加の戦闘・試合進行検証は同じコマンドに `--check Volley` / `--check Match` を指定する。レポートだけを再検証する場合は `--report-only`。UEは日本語パスを含むレポートをUTF-16で出す場合があるため、検証スクリプトはBOMを自動判別する。

## 完成した検証用配布物（2026-11-02）

- アプリ: `SavedBuilds/Mac/Package/IkebukuroNinjaCity-Mac-Shipping.app`
- ZIP: `SavedBuilds/Mac/IkebukuroNinjaCity-1.0.0-Mac-arm64-local.zip`
- SHA-256: `718396b5f7ce5e4ddabde6411d4a92033e94b4805239e417f612c51c0f83b5b5`
- 総括: `Saved/Validation/mac-release.json`
- 実機レポート: `Saved/Validation/mac-runtime-{music,volley,match}.utf8.json`
- 実機画像: `Saved/Validation/mac-runtime.png`

| 確認 | 結果 |
| --- | --- |
| Editor / Shipping C++・Cook・Package・Archive | 成功 |
| Non-UFS地図・BGM | 48ファイルのSHA-256一致 |
| 出典・ライセンス・説明 | Distribution全ファイル一致 |
| 日本語・空白のある移動先からの起動 | 成功、移動先Contentの地図2系統を使用 |
| 音楽・試合ライフサイクル | 12項目合格、曲長243.720秒、実音声デコードあり |
| 敵射撃・壁衝突・一撃敗北 | 14項目合格 |
| 入力・試合進行・3連続リトライ | 25項目合格 |
| ローカル署名整合性・ZIP CRC | 成功 |
| 同梱検証スクリプト回帰試験 | 6項目合格 |

音楽試験は敵を静止させて再生を分離し、戦闘試験は配置・固定時間刻みを制御している。実際の通し攻略・別Mac・macOS 14・OSのネットワーク切断状態は未検証。試験中はUEのHTTPを無効にした。公開向け署名・公証は証明書未設定のため未実施。`local` ZIPを公証済み版として扱わない。
