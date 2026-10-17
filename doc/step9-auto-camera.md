# Step 9 — 方向転換に追従するカメラ

作業日: 2026-10-17。Step 9への追加要望として、自動カメラを実装・検証した。Step 9全体の性能計測・最適化・通し試遊は別途行う。

## 操作と挙動

- 移動しながら方向転換すると、カメラが主人公の背後へ滑らかに回り込む。初期値は水平速度1.2m/s以上で追従、最大回転速度100度/秒。
- マウスで視点を動かしている間と、最後の操作から1.2秒は手動視点を優先。その後、移動中なら自動追従へ戻る。
- 手裏剣の投射入力から0.6秒は照準を維持する。連続投射中もその都度延長する。
- 上下の視点角度は自動では変えない。停止中・開始前・ポーズ中・勝敗確定後も自動回転しない。
- DやSを押し続けても、カメラの回転に連れて主人公が円を描かない。押したときの移動の基準方向を保持する。キーを離すか、入力方向を変えると現在の視点を基準にし直す。マウスによる視点操作中は、視点に合わせて移動方向も更新する。
- 従来のSpringArmによる壁際でのカメラ距離調整と三人称視点を維持する。建物全体の遮蔽や最適な視点角度を自動判断する機能は含まない。

`NinjaCharacter` の `Ninja | Camera` で、自動追従の有効・無効、追従の強さ、最大回転速度、手動操作後の待ち時間を調整できる。リトライでは入力方向の記憶と待ち時間をリセットする。

## 実装

`ANinjaCharacter::DoMove` で移動の基準となる水平角度を記憶し、`UpdateAutoCamera` が水平視点だけを更新する。キャラクター自体は引き続き移動方向に向く。時間差を考慮した補間と回転速度の上限を組み合わせ、角度の境界は短い側へ回る。

移動のCompleted／Canceledで方向の記憶を解放し、マウス操作・投射・ポーズ・リトライと整合させた。新規ダウンロードや追加ツールは使わない。UE 5.8・Visual Studio/MSVC・Windows SDKと通常の設定・キャッシュ・一時ファイルへのStep 9のアクセスは会話で承認済み。

## 検証

UE 5.8のEditor対象をビルド済み。30／60／120fpsの固定物理時間で各14項目成功。池袋の描画を伴う60fps設定でも同じ14項目が成功。実際にマッピングされたD/Sキーを入力し、横移動・後退で移動方向が回り続けないこと、追従の最大速度、マウス優先、投射時の待機、上下角度の維持、ポーズ、敗北、リトライを確認した。

カメラ検証は地形の影響を分離するため、開始後の移動のみFlying・キャラクター衝突なしで行う。既存の地上戦闘・実際の手裏剣・踏みつけ・3回リトライを含む池袋ゲーム進行25項目も成功した。

固定物理時間のfpsは実測の描画性能ではない。通常操作での街中・屋上・壁際・ジャンプ中の見やすさ、追従速度の好み、1080pの負荷計測は引き続きStep 9の確認対象。今回、描画品質や巨大爆発の設定は変更していない。

レポート: [30fps](validation/step9-camera-30.json)、[60fps](validation/step9-camera-60.json)、[120fps](validation/step9-camera-120.json)、[池袋描画あり](validation/step9-camera-city.json)、[ゲーム進行](validation/step9-camera-match.json)。

[池袋で自動追従した画面](validation/step9-camera-city.png)。カメラ試験時は従来の300秒規則を明示指定している。通常の池袋プレイの制限時間はBGMの約4分4秒。

## 再現手順

プロジェクトルートから、既存レポートと重複しないRunNameで実行する。

```powershell
& Tools/Editor/Build-Step1.ps1
& Tools/Editor/Verify-Step9Camera.ps1 -FPS 30 -RunName camera-30-new
& Tools/Editor/Verify-Step9Camera.ps1 -FPS 60 -RunName camera-60-new
& Tools/Editor/Verify-Step9Camera.ps1 -FPS 120 -RunName camera-120-new
& Tools/Editor/Verify-Step9Camera.ps1 -City -RunName camera-city-new
& Tools/Editor/Verify-Step8.ps1 -City -RunName camera-match-new
```

通常プレイでは起動マップ `L_Ikebukuro` でEnterを押し、Dを保持して横へ走る、キーを離してSで反転する、移動しながらマウスを動かす、投射を続ける、の順に試すと違いが分かる。
