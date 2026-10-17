# Step 10 — Windows配布ビルド

## 構成

UE 5.8のWin64 Shippingとして、起動マップを `L_Ikebukuro`、既定GameModeを `BP_NinjaTrainingGameMode` に設定。エディタを使わず、配布フォルダ直下の `IkebukuroNinjaCity.exe` から起動する。

ゲーム用アセットをCookし、Pak / IoStoreへ収録する。実行時にファイルとして開くデータはNon-UFSで同梱する。

| 対象 | 配布物内の場所 |
| --- | --- |
| テクスチャ付き建物 | `IkebukuroNinjaCity/Content/CityData/IkebukuroLOD2/Buildings/` |
| 道路 | `IkebukuroNinjaCity/Content/CityData/Ikebukuro/Roads/` |
| BGM | `IkebukuroNinjaCity/Content/Movies/Ikebukuro Ninja City.m4a` |
| 操作説明・クレジット | 直下の `README-ja.md`、`CREDITS.md`、`Licenses/` |

旧LOD1建物、豊島区全域の実証用データ、原本ZIP、ソース、Git、ツールを配布データへ追加しない。動的ロードする忍者マテリアル・SE・BasicShapesはCookを明示する。地図と音楽のパスは実行時のProjectContentDirから解決する。

## 再作成

プロジェクトルートで次を実行する。外部ツールへのアクセスは会話で承認された範囲に限る。

```powershell
./Tools/Editor/Package-Step10.ps1
./Tools/Editor/Prepare-Step10.ps1 -Relocate -RelocationName 'Relocated Windows v2'
./Tools/Editor/Verify-Step10.ps1 -Check Music -RunName step10-music-full-r2 -FullTrack -PackagePath 'SavedBuilds/Step10/Relocated Windows v2'
./Tools/Editor/Verify-Step10.ps1 -Check Volley -RunName step10-volley -PackagePath 'SavedBuilds/Step10/Relocated Windows v2'
./Tools/Editor/Verify-Step10.ps1 -Check Match -RunName step10-match -PackagePath 'SavedBuilds/Step10/Relocated Windows v2'
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Editor/finalize_step10.py
```

既存の検証レポート、コピー先、ZIPは上書きしない。再実行する検証は別RunNameを使い、配布再作成時の保存先も明示的に管理する。

## 出典と取得範囲

配布説明の原本は `Content/Distribution/`。地図の出典・切り出し等の加工を明記し、Cesiumの付属LICENSE・ThirdParty表記・ヘッダー内のMikkTSpace/HTML Tidy表記を保存した。

公式カタログが指定したPLATEAUサイトポリシーを、ユーザーの明示承認後に取得して `doc/references/` へ保存した。[取得記録](references/plateau-site-policy-source.md) を参照。新たな画像・都市データ・コードは取得していない。BGMはユーザー提供品で、作者・公開配布許諾の情報は未登録。

## 検証の範囲

2026-10-17にWin64 Shippingのビルド・Cook・Pak/IoStore・Archiveが成功。インストール版UEの共有ビルドではTargetからのプラグイン有効/無効指定が使えないため、その指定は採用していない。Cook時のエディタ用ツール無効化を指定した。

Shippingのunityコンパイルで、Cesiumのネイティブ依存が露出するWindowsの `min` / `max` / `DrawText` マクロがUEのRHI・HUDと衝突した。既存のGetObject対策と同じ境界でundefし、エンジン・プラグイン本体を変更せず解消した。

| 検証 | 結果 | 記録 |
| --- | --- | --- |
| 地図・BGM・配布説明のNon-UFS同梱 | 55ファイルすべて元ファイルとSHA-256一致 | `validation/step10-staging.json` |
| 別フォルダへのコピー | 配布マニフェストの93ファイルすべてSHA-256一致 | `validation/step10-relocation.json` |
| 起動と地図 | EXEのみ、マップ指定なし、別の作業ディレクトリから起動。Cook済みL_Ikebukuro、地図2系統をコピー先Contentから解決 | `validation/step10-music-full.json` |
| 全曲再生 | 243.718994秒、実音声デコード、ポーズを含む初回再生244.536秒。曲末敗北、再試合、即死で停止、曲末と最終撃破の競合の12項目が成功 | 同上 |
| 赤忍者の攻撃 | 100m境界、±30度3発、2秒間隔、壁・味方・一撃敗北・再試合の14項目が成功 | `validation/step10-volley.json` |
| ゲーム進行 | Enter/P/Fの入力、ポーズ、勝敗、実弾・踏みつけ、3連続リトライの25項目が成功 | `validation/step10-match.json` |
| ZIP | 全エントリのCRC確認成功 | `validation/step10-release.json` |

音楽の初回長時間試験は64.6秒で想定外の敗北となった。旧レポートには敗北理由がなく原因は断定できないため、検証時の手動移動・視点入力を抑制し、敗北理由・位置・体力の記録を追加して再実施した。再試験は曲末まで完走した。通常プレイの入力にはこの抑制を適用しない。

音楽検証は実時間・実WMF・実オーディオミキサーで実行し、敵を停止して音楽のライフサイクルを分離している。戦闘検証は実AIと実弾を使うが、射程や衝突を調べるため配置を制御する。ゲーム進行の固定時間検証は旧300秒タイマーを短縮して境界を検証し、BGMの実際の終了は全曲検証で別に確認した。

検証コマンドではUEのHTTPを無効化し、地図・音楽はローカルファイルだけで動作した。ローカルHTTPサーバー・開発用の絶対パス・エディタ起動は実行に不要。ビルド時のUE Zenキャッシュサービスは配布版の起動には使用しない。

配布画像も確認し、同梱した建物テクスチャが表示されている。元のユーザー変更である `.uproject` の差分とルートの原本M4Aは保持し、この工程のコミットに含めない。

## 成果物

- フォルダ：`SavedBuilds/Step10/Package/Windows/`
- 起動：同フォルダの `IkebukuroNinjaCity.exe`
- ZIP：`SavedBuilds/Step10/IkebukuroNinjaCity-1.0.0-Windows.zip`
- サイズ：638,488,910 bytes（マニフェスト自身を除く）、ZIP 455,539,174 bytes
- ZIP SHA-256：`e0f32b6b7d937ccfafefaf8df94f4eeb5845e71d281f6795d5e4eda636190e3f`
- Windows向けVC++ランタイム `Engine/Extras/Redist/en-us/vc_redist.x64.exe` を同梱。自動インストールは行わない。
- 画像：`Saved/Validation/step10-music-full-r2.png`、`step10-volley-fan.png`、`step10-volley-one-hit.png`、`step10-match_*.png`
- 開発用PDBの重複は配布から除外し、元のシンボルはプロジェクトのBinaries内に保持。

別PCでの動作確認、OSのネットワーク切断状態での試験、初見の人による通し試遊は未実施。自動戦闘検証では敵や主人公を配置して状態を制御するため、実際の攻略時間や難易度の評価は含まない。
