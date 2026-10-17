# Step 6.5 — 街のテクスチャ表示

実装日: 2026-10-16。元ZIPに含まれる建物LOD2を使い、`L_Ikebukuro` の建物描画と固定足場を更新した。追加の画像取得やオンライン地図は使っていない。

後続変更: [Step 7](step7-match-flow.md)で通常の池袋Playに開始画面・300秒・勝敗・リトライを追加した。Enterで開始し、Pでポーズする。以下の表示比較マップと専用検証は従来どおり利用できる。

## 試す場所

通常どおり `L_Ikebukuro` を開いてPlayする。窓・看板などの画像がある建物を表示し、写真のない面は元データの色で表示する。道路と補完地面は従来の表示を使う。操作と敵3体の構成はStep 6と同じ。

比較用に `/Game/Ninja/Maps/L_IkebukuroLOD1Comparison` を保存した。こちらは更新前のLOD1と足場を持つ。起動マップは `L_Ikebukuro` のまま。

## データと表示

`Tools/City/crop_ikebukuro.py --textured` は、Step 4と同じ400m四方のプレイ範囲と150mの背景バッファを使い、別の `Content/CityData/IkebukuroLOD2/Buildings/` へ切り出す。従来のLOD1・道路データと原本ZIPを保持する。

| 項目 | 結果 |
| --- | ---: |
| 元LOD2タイル | 829 |
| 選択した葉タイル | 30 |
| 選択タイルの合計サイズ | 60,308,572 bytes（約60.3MB） |
| 埋め込み画像／テクスチャ | 30／30 |
| 埋め込み画像データ | 27,928,676 bytes |
| 画像を参照するプリミティブ | 66 |
| 画像なしのプリミティブ | 71 |

プリミティブは描画単位で、建物数・面積の比率ではない。全建物・全ての面を写真で覆うデータではなく、近づくと画像のぼけや撮影時の遮蔽物が見える場合がある。写真のない面に新しい写真を生成・取得する工程は含めていない。

b3dm／GLBを元ZIPからバイト単位でそのままコピーして画像・UV・マテリアルを保持し、SHA-256をレポートに残す。画像はWebP形式の埋め込みで、`EXT_texture_webp` の参照を保持する。外部ファイル参照を拒否し、テクスチャのあるプリミティブに対応するUV、画像参照、画像のバッファ範囲を検査する。

## 建物の重複と足場

LOD1とLOD2の建物IDを照合し、LOD2側で実際に使うIDをLOD1の補完候補から除外する処理を用意した。Draco圧縮をCesiumのデコーダーで展開し、各三角形の `_BATCHID` とバッチテーブルの `gml_id` を対応させる。

選択したLOD2は1,378個の建物IDを含む。粗いLOD1タイルにはそれ以外のIDもあるが、今回の背景矩形内に必要な補完三角形は0だった。そのため現在はLOD2だけを描画し、LOD1の補完メッシュは置いていない。IDの重複は0。将来範囲を変えた場合は、必要な補完があれば単色のメッシュとして生成する。

建物の固定足場はLOD2から `/Game/Ninja/City/SM_BuildingsLOD2Collision` に再生成した。保存した三角形は73,034個。描画用Cesiumの物理判定は通常のPlayでは無効にし、読み込み・画面外・LOD変更に左右されない固定足場を使う。元の `SM_BuildingsCollision` は比較マップ用に残した。

以前の屋上ターゲット3点はLOD2で約50・60・166cm高くなったため、保存した三角形から直接高さを求めて、ターゲットと目印を更新した。屋上の敵と巡回領域の基準高も約30cm上げた。エディタ起動直後の未準備な物理ワールドに依存して配置を決めない。

## 再生成

UEエディタを保存して終了し、プロジェクトルートで実行する。エージェントが行う場合は、この工程の外部ツールアクセスに対する承認が必要。

```powershell
.\Tools\Editor\Build-Step1.ps1
.\Tools\Editor\Setup-Step6_5.ps1
.\Tools\Editor\Verify-Step6_5.ps1 -Baseline -RunName my-lod1-comparison
.\Tools\Editor\Verify-Step6_5.ps1 -RunName my-lod2-textures
.\Tools\Editor\Verify-Step4.ps1 -RunName my-lod2-city
.\Tools\Editor\Verify-Step6.ps1 -City -FPS 60 -RunName my-lod2-encounter-60
.\Tools\Editor\Verify-Step6.ps1 -City -FPS 30 -RunName my-lod2-encounter-30
```

比較マップは最初の実行時だけ複製する。`-BaselineOnly` を付けたセットアップは比較マップだけを準備する。レポート名は既存ファイルと重ならないものを指定する。

元データはGit対象外なので、ソースとマップだけを別環境に移しても画像は揃わない。原本ZIPとCesiumプラグインを用意して再生成する。既存の `DirectoriesToAlwaysStageAsNonUFS=CityData` により配布時の同梱対象となるが、Step 6.5では配布ビルドの作り直しは行っていない。オフラインの配布確認はStep 10で行う。

## 検証の範囲

`ACityTextureCheck` は `-CityTextureCheck` がある起動だけで有効になる。地上・俯瞰・屋上の同じ視点を比較し、描画メッシュ数、Cesiumが生成した画像テクスチャ数、プロセスの物理メモリ、フレーム時間の参考値を記録する。画面キャプチャでも実際に写真が表示されることを確認する。画像読み込みのカウントは、マテリアルの宣言一覧ではなく実際に生成された `CesiumRuntimeTexture` を対象にする。`texture_object_reported_bytes` はUObjectが報告するサイズで、Cesium独自のGPU画像メモリを含まず、VRAM使用量として扱わない。

表示比較中は敵の移動・衝突を無効化する。敵が動く状態での巡回・追跡・戦闘はStep 6の遭遇検証を別に実行する。

性能の値は1280×720、内部解像度80%、60fps制限、低い描画設定のエディタ用ゲーム実行で、起動後のストリーミングも含む。製品の1080p・60fps達成を示すベンチマークではない。仮想シャドウマップのジョブキュー警告があり、品質と性能の調整はStep 9で扱う。

### 結果

最終ビルド、切り出し・画像参照検証、表示比較、足場と遭遇の回帰確認が成功した。

| 確認 | 保存したレポート |
| --- | --- |
| 切り出し・画像30点・参照・ハッシュ | [データ](validation/step6_5-crop.json) |
| LOD2足場・屋上配置・重複ID 0 | [セットアップ](validation/step6_5-setup.json) |
| LOD1比較 | [表示](validation/step6_5-lod1.json) |
| LOD2の画像30点・描画137メッシュ | [表示](validation/step6_5-lod2.json) |
| 走行・3段ジャンプ・屋上3か所・カメラ | [足場](validation/step6_5-city-regression.json) |
| 敵の巡回・追跡・帰還・両方の撃破・60Hz | [遭遇](validation/step6_5-encounter-60.json) |
| 同上・30Hz | [遭遇](validation/step6_5-encounter-30.json) |

121地点で描画用形状と固定足場を照合し、高さの最大差は約0.0021cm。描画タイルを取り外しても足場は維持された。敵の通常移動中の安全復帰は0回で、検証時に意図的に屋上からずらした場合だけ1回復帰した。

| 参考値 | LOD1 | LOD2 |
| --- | ---: | ---: |
| 生成されたCesium画像 | 0 | 30 |
| 建物の描画メッシュ数の最大 | 4 | 137 |
| 測定終了時のプロセス物理メモリ | 約3.46GB | 約5.23GB |
| 計測フレーム時間の平均 | 17.22ms | 17.35ms |
| 最大フレーム時間（起動時を含む） | 392.53ms | 400.00ms |

LOD2はメモリ使用量が増える。フレーム時間は60fps制限の影響があるので、性能に余裕があるという判定には使わない。

比較画像: [LOD1・地上](validation/step6_5-lod1-street.png) ／ [LOD2・地上](validation/step6_5-lod2-street.png)、[LOD1・俯瞰](validation/step6_5-lod1-overview.png) ／ [LOD2・俯瞰](validation/step6_5-lod2-overview.png)、[LOD1・屋上](validation/step6_5-lod1-roof.png) ／ [LOD2・屋上](validation/step6_5-lod2-roof.png)。

## アクセスの記録

UE 5.8・Visual Studio／MSVC・Windows SDKの読み取り・実行と、通常の設定・キャッシュ・一時ファイルへのアクセスについて、Step 6.5用の明示承認を得た。成果物とログはプロジェクト内に保存。今回は追加のダウンロード・Gitリモート接続を行っていない。
