# Step 7 — 300秒・勝敗・ポーズ・リトライ

追加ルール（2026-10-17）: 現在は敵の移動距離・速度2倍、100m以内の3方向射撃、黒忍者の一撃敗北を採用。[最新仕様と検証](one-hit-volley.md)。以下は当時の実装記録。

実装日: 2026-10-17。テクスチャ付き `L_Ikebukuro` にゲーム進行を組み込んだ。今回は地上2体・屋上1体を対象とする。8体の配置と3〜5分の難易度調整はStep 8で行う。

## 遊び方

`L_Ikebukuro` を開いてPlayする。街の描画、主人公の足場、敵の着地と巡回経路が揃うまで待ち、操作説明の画面でEnterを押す。3秒のカウントダウン後に操作と300秒タイマーが始まる。

| 操作 | 入力 |
| --- | --- |
| 開始／結果画面からリトライ | Enter |
| 移動／視点 | WASD／マウス |
| 走る | Shift |
| ジャンプ／頂点付近で次の段 | Space |
| 手裏剣 | 左クリックまたはF |
| ポーズ／再開 | P |
| 安全な路面への復帰 | R（プレイ中） |

単独ゲーム実行ではEscでもポーズできる。エディタのPIEはEscをPlay終了に使う場合があるため、ポーズ確認にはPを使う。

右上に残り時間・残敵数・体力を表示する。赤いマーカーは敵の方向、距離、主人公から見た高さを表す。画面外や建物の向こうの敵も探せるよう表示し、最後の1体にも残る。窓や壁が手裏剣を遮るルールは従来どおり。

全滅させると勝利。時間切れまたは体力0で敗北する。体力0で自動回復する旧試作の挙動は、このゲームモードでは使わない。結果画面には撃破数、踏みつけ・手裏剣の内訳、残り時間を表示する。Enterで敵・主人公・時間を初期化し、再び操作説明から始められる。

軽い専用マップ `/Game/Ninja/Maps/L_NinjaMatch` でも同じ進行を試せる。旧 `L_NinjaCombat`、`L_NinjaEncounter`、LOD1比較マップは従来の練習用として残している。

## 状態と判定

`ANinjaMatchDirector` が `Loading → Ready → Countdown → Playing → Won / Lost` を管理する。ポーズはCountdown／Playingに対するUEのワールドポーズを使い、物理移動と手裏剣も止める。ポーズ解除のキーは停止中も受け付ける。

Loading中は描画タイルの準備、主人公の固定足場、敵の着地・歩行グラフを確認する。0.5秒間準備が維持されてからReadyへ進む。ReadyとCountdownではプレイ時間を減らさず、移動・ジャンプ・攻撃・被ダメージを受け付けない。

敵の撃破通知は、そのラウンドで登録した敵だけを一度ずつ受け付ける。結果は物理・投射物の更新後に次の順で判定する。

1. 全敵撃破なら勝利。
2. 主人公の体力0なら敗北。
3. 残り時間0なら敗北。

同じフレームの最終撃破と時間切れは勝利を優先する。この順序では最終撃破と体力0の同時成立も勝利になる。結果確定後は移動を止め、飛んでいる手裏剣を消し、撃破・被ダメージ・新しい投射を拒否する。結果確定前に発生した同フレームの命中と、結果確定後の遅れた命中を区別する。

リトライはレベルを再読み込みせず、保存した初期配置・クラス・タグ・巡回領域で敵を作り直す。主人公の体力、撃破数、投射数、ジャンプ、無敵時間、投射待ち時間、速度と保留中の力をリセットする。都市タイルと固定足場を保持し、開始前の準備確認をもう一度行う。

## 実装範囲

- `NinjaMatchDirector.*`: 準備確認、状態、入力、タイマー、撃破登録、勝敗と再初期化。
- `NinjaCharacter.*`: ゲーム状態に応じた操作・ダメージ制御、試合用の初期化。
- `RedNinja.*`、`NinjaShuriken.cpp`: 開始前・ポーズ中・結果確定後の命中制御。敵は準備中も着地と経路構築を済ませ、プレイ開始まで巡回しない。
- `NinjaTrainingHUD.*`: 説明、カウントダウン、時間・残敵数、方向マーカー、ポーズ、結果とリトライ案内。
- `CityPlayArea.cpp`: 試合の操作が許可される間だけRや落下・境界からの復帰を行う。
- `Tools/Editor/setup_step7.py`: 池袋と専用マップに進行管理と検証用アクターを配置する。

Step 4〜6.5の専用検証フラグでは進行管理を無効化し、従来の足場・移動・戦闘・表示の検査を継続できる。通常のPlayと `-NinjaMatchCheck` では新しい進行が有効。

## 再生成と検証

UEエディタを保存して終了し、プロジェクトルートで実行する。レポートは既存ファイルの上書きを拒否するため、再実行時は別の `RunName` を指定する。

```powershell
.\Tools\Editor\Build-Step1.ps1
.\Tools\Editor\Setup-Step7.ps1
.\Tools\Editor\Verify-Step7.ps1 -FPS 60 -RunName my-match-60
.\Tools\Editor\Verify-Step7.ps1 -City -FPS 60 -RunName my-city-60
.\Tools\Editor\Verify-Step7.ps1 -City -FPS 30 -RunName my-city-30
.\Tools\Editor\Verify-Step7.ps1 -City -FPS 30 -FatalFall -RunName my-city-fall-30
.\Tools\Editor\Verify-Step5.ps1 -RunName my-combat-regression
.\Tools\Editor\Verify-Step6.ps1 -City -RunName my-encounter-regression
.\Tools\Editor\Verify-Step4.ps1 -RunName my-city-regression
```

`ANinjaMatchCheck` は専用フラグがある実行だけで動く。Enter・P・Fを実際の入力経路へ送り、ポーズ中の時間・敵・手裏剣の停止、最後の1体の画面外マーカー、最終撃破と期限の競合、結果確定後の命中拒否、勝利・時間切れ・体力0からの3回連続リトライを確認する。最後のラウンドでは実際の直進弾と下降中の踏みつけで、進行管理への撃破反映も確認する。

期限の境界テストは、初期値300秒と開始前の時間保持を確認した後、検証時だけ残り時間を0.15秒に短縮して通常の更新で時間切れにする。最終撃破との競合は同じフレームで残り時間0と撃破を発生させる。通常のゲーム設定は300秒のままであり、検証のために毎回実時間5分を待つものではない。

`-FatalFall` の実行では、接触による最後のダメージの代わりに、体力1の主人公をマップ下へ移動させる。通常の落下復帰処理が体力を0にし、回復せず敗北してから再試行できることを確認する。

撃破や結果の個別確認中は敵を一時停止して条件を分離する。敵を動かした巡回・追跡・帰還の回帰検証も別に行う。固定時間刻み30／60Hzの結果は、実際の描画性能の保証ではない。配布ビルドの更新はStep 10で行う。

### 保存した結果

最終ビルドは成功。専用マップ60Hz、池袋60／30Hz、池袋の致命的落下30Hzで進行の25項目が通過した。各実行で4ラウンドと3回連続のリトライを確認した。開始説明、カウントダウン、ポーズ、最後の1体のマーカー、勝利と2種類の敗北画面もキャプチャで確認した。

| 確認 | レポート |
| --- | --- |
| マップへの配置 | [セットアップ](validation/step7-setup.json) |
| 専用マップ・60Hz | [進行](validation/step7-match-60.json) |
| 池袋・60Hz | [進行](validation/step7-city-60.json) |
| 池袋・30Hz | [進行](validation/step7-city-30.json) |
| 体力1からの致命的落下・池袋30Hz | [進行](validation/step7-city-fall-30.json) |
| 既存の命中・踏みつけ・無敵時間・二重加算防止 | [戦闘回帰](validation/step7-combat-regression.json) |
| 敵の巡回・追跡・帰還・屋上・両方の撃破 | [AI回帰](validation/step7-encounter-regression.json) |
| 走行・3段ジャンプ・3か所の屋上着地・固定足場・カメラ | [地図回帰](validation/step7-city-regression.json) |

画面: [開始](validation/step7-city-ready.png)、[カウントダウン](validation/step7-city-countdown.png)、[ポーズ](validation/step7-city-paused.png)、[最後の敵](validation/step7-city-last_enemy.png)、[勝利](validation/step7-city-victory.png)、[時間切れ](validation/step7-city-timeout.png)、[体力0](validation/step7-city-health_loss.png)。

詳細ログは `Saved/Logs/Step7-*`、調整中を含む実行レポートと画面は `Saved/Validation/step7-*` に保存した。`doc/validation/` には完成版の成功レポートを残した。変更前のローカルコミットは `bbde65e`。

## アクセス

Step 7のためのUE 5.8・Visual Studio／MSVC・Windows SDKの読み取り・実行と、通常の設定・キャッシュ・一時ファイルへのアクセスはユーザー承認済み。新しいダウンロード・外部素材・Gitリモート接続は使っていない。成果物、ログと比較画像はプロジェクト内に保存する。
