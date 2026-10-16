# 地図読み込みの検証記録（Step 2）

実施日: 2026-10-16。Step 2 の読み込み・衝突・Windows同梱検証を完了。

## 原本と展開

- 原本: `13116_toshima-ku_pref_2025_3dtiles_mvt_1_op.zip`
- 選択: `13116_toshima-ku_pref_2025_citygml_1_op_bldg_3dtiles_13116_toshima-ku_lod1/`
- 展開先: `Data/City/ToshimaLOD1/`
- ファイル数: tileset.json 1件 + b3dm 68件 = 69件
- 展開サイズ: 352,726,159 bytes
- 展開手順: `Tools/City/Extract-Lod1.ps1`
- 参照とヘッダ検証: `Tools/City/inspect_lod1.py`
- 検証レポート: `Saved/Validation/step2-lod1-extraction.json`、`step2-lod1-inspection.json`

68件全てのタイル参照がローカルで解決し、b3dmヘッダの形式・長さと埋め込みGLBの存在を確認した。各メッシュの描画や地理的な配置の正しさは、この構造検査には含まない。

## 座標と構造

| 項目 | 値 |
| --- | --- |
| 3D Tiles | 1.0 |
| refine | REPLACE |
| 西端経度 | 139.67767285814747度 |
| 南端緯度 | 35.71243247434936度 |
| 東端経度 | 139.75262979713403度 |
| 北端緯度 | 35.7459333973564度 |
| root region の高さ範囲 | 49.34219309826564〜217.119019458607 m |
| root transform | なし |

regionの水平角度はラジアンから度に変換した。高さ範囲はデータ内の境界値であり、ビルの高さそのものではない。基準点と実際の道路・建物底面の標高はエディタで検証する。

## 読み込み方式の候補

第一候補は Cesium for Unreal。UE 5.8インストール内にCesiumの `.uplugin` は見つからなかった。

保存した公式リリース情報では、v2.30.0がUE 5.6 / 5.7 / 5.8に対応する。UE 5.8向けの完成版ZIPは以下。

- ファイル: `CesiumForUnreal-58-v2.30.0.zip`
- サイズ: 1,262,445,524 bytes
- SHA-256: `f523862a9ec712801d024c488f8a73be18c5dc9be9ba995ac05761886a0b4bbf`
- [公式リリース](https://github.com/CesiumGS/cesium-unreal/releases/tag/v2.30.0)

ユーザーから取得・配置・使用の明示承認を得た。ZIPを `Downloads/` に保存し、SHA-256が上記の公式値に一致することを確認した。`Plugins/CesiumForUnreal/` に展開し、UE本体には追加していない。公式ZIPの展開後サイズは約5.88GB（複数プラットフォーム向けファイルを含む）。プラグインのコード自体は変更していない。

採用版と取得元は `doc/third-party/cesium-plugin.json`、Apache 2.0のライセンス本文は `doc/third-party/Cesium-LICENSE.txt` に保存した。依存物はプラグイン内の `ThirdParty.json` と各同梱ライセンスを参照する。

## 実装した読み込みとマップ

- C++: `Source/IkebukuroNinjaCity/City/LocalCityTileset.*`
- 検証マップ: `Content/Ninja/Maps/L_CityProof.umap`
- マップ作成・更新: `Tools/Editor/setup_step2.py`
- 起動時に `FPaths::ProjectContentDir()` からローカルtilesetのURLを再構成する。マップに保存された開発時の絶対URLに依存しない。
- 配布用の都市データは `Content/CityData/ToshimaLOD1/` に配置する。`DirectoriesToAlwaysStageAsNonUFS` に `CityData` を指定し、パッケージでも外部ファイルとして読み込める状態で同梱する。
- Cesium ion の地図・オンライン地形・トークンは使用しない。

基準点は池袋駅付近の仮の位置として経度139.7109度、緯度35.7295度、高さ60mに固定した。この高さは地理データの座標系における基準値であり、地面から60mの意味ではない。正確な駅中心の測量や道路面の高さ合わせは後続工程に残す。

座標変換の実測:

| 入力の変化 | UE側の差 |
| --- | --- |
| 高さ +1m | Z +99.99999994cm |
| 経度 +0.00001度 | X +90.47144cm |
| 緯度 +0.00001度 | Y -110.95504cm |

従って、この設定では X が東、Y が南、Z が上。測定レポートは `Saved/Validation/step2-map-setup.json`。

マップには仮地面、幅20mの展望デッキ、1mの立方体と長さ10mの梁、Third Person の主人公を置いた。Playは地面高さの調整が不要な展望デッキから開始する。建物LOD1の表示検証用であり、道路・地形・テクスチャを含む完成した街並みではない。区全体のデータを読み込んでおり、池袋周辺だけの空間的な切り出しは Step 4 で行う。

## カメラとLODによる衝突の検証

最初は原本の階層tilesetを使用した。表示・縮尺・建物へのトレースは成功したが、180度カメラを反転した後、49本の垂直トレースのうち建物に当たった本数が31本から28本に減った。フラスタム・霧によるカリングを無効にするだけでは、今回の条件で同じ足場を保てなかった。

そこで検証用に、最細の葉タイル49個を固定で選ぶ `tileset-fixed.json` を生成した。

- `Tools/City/prepare_fixed_lod.py` で原本から再生成できる。
- 原本の `tileset.json` と `.b3dm` は変更しない。
- このデータに階層transformがないことを確認し、transformを持つデータでは処理を拒否する。
- rootのcontentを外し、葉タイルを直接のchildrenとして保持する。葉のgeometricErrorを0にし、rootから必ず詳細化する設定にする。
- 検証マップは固定版を参照し、フラスタム・霧・オクルージョンによるタイルカリングを無効にする。

固定版では表示メッシュ49個、読み込み100%、カメラ反転前後とも衝突31点を確認した。結果は `Saved/Validation/step2-city-fixed-runtime.json` と同名のPNG。これは限定された位置・時間での検証であり、街全体の常時ロードを最終ゲームの性能方針にはしない。Step 4で範囲を縮小し、プレイ用の固定コリジョンと最終的なLOD・表示距離を決める。

## ビルド時の対応

- UE 5.8.2 / MSVC 14.51 / SDK 10.0.26100.0 でプラグインとプロジェクトをビルドした。
- ユーザーのエディタがDLLを保持していたため、保存・終了を依頼してからリンクを再実行した。強制終了はしていない。
- Windows配布ビルドで、ネイティブ依存からの `GetObject` マクロがUEの `TScriptInterface::GetObject` と衝突した。プロジェクト側ヘッダで当該マクロだけを解除した。UE本体やプラグインのソースは変更していない。
- UBA保存先は引き続き `Saved/UnrealBuildAccelerator` を使う。
- Gitはユーザー・システム設定を読まない設定で使用する。リモートは設定しない。

## 再現手順

1. 承認済みZIPを展開するか、既存の `Plugins/CesiumForUnreal/` を使う。
2. `Tools/City/Extract-Lod1.ps1` を実行する。
3. UE同梱Pythonで `Tools/City/inspect_lod1.py`、`Tools/City/prepare_fixed_lod.py` を実行する。
4. `Tools/Editor/Build-Step1.ps1` でエディタをビルドする。
5. UE PythonScript commandletで `Tools/Editor/setup_step2.py` を実行する。
6. `Tools/Editor/Verify-Step2.ps1` で実行時の表示・衝突・縮尺・屋上着地を検証する。
7. `Tools/Editor/Package-Step2.ps1` で `SavedBuilds/Step2/` にWindows版を生成する。

UE・コンパイラ等の実行は会話で承認された範囲に限る。追加取得は別途承認を得る。パスや手順の再現のため、原本・プラグイン・配布データはGitから除外しているが、このフォルダ内に保持する。

## 最終結果

| 確認項目 | UE実行ファイルのゲームモード | Windowsパッケージ（移動後） |
| --- | --- | --- |
| 判定 | passed | passed |
| 読み込んだ表示メッシュ | 49 | 49 |
| 読み込み進捗 | 100% | 100% |
| 建物へ当たったトレース数（反転前 / 後） | 31 / 31 | 31 / 31 |
| 実キャラクターの建物屋上への着地 | 成功 | 成功 |
| 1mのUE換算 | 約100cm | 約100cm |
| 地図の読み込み元 | プロジェクトのContent | 移動後の配布物自身のContent |

屋上着地は、読み込み後に建物へのトレースで屋上位置を取り、その約3m上へ主人公を移し、落下後に屋上の高さで停止することを検証した。仮の地面への着地だけで成功とは判定していない。

Windowsパッケージは `SavedBuilds/Step2/Windows` から `SavedBuilds/Step2/VerifiedWindows` に移動して確認した。都市データ70ファイルが元と同じサイズで同梱され、実行時の参照先がこの移動後フォルダの中であることも確認した。HTTPを無効にした起動引数を使用し、オンライン地図・ローカルHTTPサーバー・エディタのデータ参照を必要としない。

最初の配布検証では同梱設定の名前を誤り、実行時に地図が404となる問題を検出した。`DirectoriesToAlwaysStageAsNonUFS` に修正して同梱をやり直し、移動後の実行検証で解消を確認した。以後は `Package-Step2.ps1` もアーカイブ内の全都市ファイルの存在とサイズを検証する。初回の失敗版 `SavedBuilds/Step2/RelocatedWindows` は検証用の旧出力であり、使うのは `VerifiedWindows` とする。

保存した証拠:

- `doc/validation/step2-map-setup.json`
- `doc/validation/step2-city-final-editor.json`
- `doc/validation/step2-city-packaged-verified.json`
- `Saved/Validation/step2-city-packaged-verified.png`（配布版の実画面）
- `Saved/Logs/Step2-Package-Retry-Console.log`（ビルド・クック成功）
- `Saved/Logs/Step2-Package-Restage-Console.log`（同梱修正後のアーカイブ成功）
- `Saved/Logs/step2-city-packaged-verified.log`（移動後の実行）

## 試す方法と次の工程

エディタでプロジェクトを開くと `L_CityProof` が開く。Playすると展望デッキから通常のThird Person操作で街を眺められる。従来の `L_TestArena` も `Ninja/Maps` に残してある。

エディタなしで試す場合は `SavedBuilds/Step2/VerifiedWindows/IkebukuroNinjaCity.exe` を起動する。配布フォルダ全体を保持すること。現在のゲームは地図検証段階であり、忍者の専用操作・敵・制限時間はまだ追加していない。

次は計画の Step 3「灰色のステージで移動とタイミング式3段ジャンプを作る」。Step 4では池袋周辺への範囲削減、道路と地形の補完、経路上の固定コリジョン、描画負荷の調整を行う。今回の固定LOD・区全体の読み込み・仮地面は検証用とする。
