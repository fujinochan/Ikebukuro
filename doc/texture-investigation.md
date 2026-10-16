# 街のテクスチャ調査

調査日: 2026-10-16。ゲームやマップの設定変更は行っていない。

後続工程: この調査後、[Step 6.5](step6_5-textures.md)で池袋範囲のテクスチャ付きLOD2を組み込み、マップと固定足場を更新した。以下は切り替え前の調査記録。

## 原本ZIPの確認結果

`13116_toshima-ku_pref_2025_3dtiles_mvt_1_op.zip` 内の建築物b3dmを全件読み、埋め込みGLBのJSONにある `images` と `textures` を数えた。画像はGLBに埋め込まれるため、ZIPの拡張子一覧にJPEGがなくても、テクスチャがないとは判断できない。

| 建築物レイヤー末尾 | b3dm数 | images数 | textures数 |
| --- | ---: | ---: | ---: |
| `lod1` | 68 | 0 | 0 |
| `lod2` | 829 | 235 | 235 |
| `lod2_no_texture` | 829 | 0 | 0 |

現在のゲームはStep 4で選んだLOD1を描画しているため、建物の写真テクスチャが表示されていない。原本にテクスチャ付きLOD2があるので、新しい外部画像を取得せず試せる。

235は建物数ではなくGLB内の画像エントリの合計。池袋のプレイ範囲での被覆、各建物の画質、道路の見た目は今回未検証。全建物が写真付きになるとは限らない。

次の実装候補は、元ZIPからプレイ範囲のLOD2を抽出して描画を切り替えること。LOD1との二重描画を避け、固定足場との高さ・壁面の差を確認する。表示用LOD2を変えても、現在の固定足場が自動的にLOD2の形状になるわけではない。

## Google Street View

技術的には画像をモデルへ投影することは考えられるが、撮影位置・向きの合わせ込み、遠近の補正、車や街路樹の遮蔽、屋上の不足を処理する必要がある。

Google Maps Platformの通常規約3.2.3(a)〜(c)では、サービス外へのコンテンツ抽出・保存、許可のないキャッシュ、Google Mapsコンテンツに基づくコンテンツ作成を制限している。このため、Street View画像を切り出してPLATEAU建物のテクスチャとしてゲームへ同梱する案は、通常のAPI利用許諾で進められる前提にしない。

Googleの別サービスであるPhotorealistic 3D Tilesは、画像付きメッシュを配信する公式API。Cesium for Unrealの対応が公式資料にあるが、APIキー、課金アカウント、帰属表示などが必要になる。これはStreet View画像を自由に抜き出す方法ではなく、ローカルのPLATEAUデータを使う現在の構成とは別の選択肢。

まず同梱LOD2の表示・範囲・画質を確認する方針を推奨する。

## 公式資料と承認

今回、Google・PLATEAU等の公式資料を取得して `doc/references/` に保存することについて、ユーザーの明示承認を得た。外部画像・地図本体の取得は行っていない。

- [Google Maps Platform利用規約](https://cloud.google.com/maps-platform/terms) — `references/google-maps-platform-terms.html`
- [Street View Static APIポリシー](https://developers.google.com/maps/documentation/streetview/policies) — `references/google-streetview-policies.html`
- [Photorealistic 3D Tiles](https://developers.google.com/maps/documentation/tile/3d-tiles) — `references/google-photorealistic-3d-tiles.html`
- [豊島区2025年度カタログ](https://www.geospatial.jp/ckan/dataset/plateau-13116-toshima-ku-2025) — API取得先 `https://www.geospatial.jp/ckan/api/3/action/package_show?id=plateau-13116-toshima-ku-2025`
