# 撃破・戦闘不能時の大爆発

作業日: 2026-10-17。Step 8への追加要望。

## 動作

- 赤忍者は手裏剣・踏みつけのどちらで倒しても、その場で大爆発して消える。
- 黒い主人公は体力0で大爆発して姿を消す。通常の被弾や曲終了だけでは爆発しない。落下による体力0も対象。
- 明るい閃光、膨らむ橙色の火球、飛び散る火花、円形の衝撃波、上昇する煙を約1.45秒で表示する。火花・衝撃波の広がりは半径約4〜5m。
- 自作の約1.15秒の爆発音を再生する。既存SEとBGMも引き続き動作する。
- 爆発は演出のみで、周囲へのダメージ、吹き飛ばし、物理衝突はない。同じ忍者への撃破の重複入力でも爆発を重複発生させない。
- 勝敗は即時に確定する。最後の撃破や主人公の戦闘不能を見せるため、結果パネルだけ0.7秒後に表示する。音楽は従来どおり勝敗確定時に停止する。
- ポーズでは演出の時間も止まる。リトライは進行中の爆発を消し、主人公の表示と体力を復元する。

## 実装

`ANinjaExplosion` は倒された忍者とは別のActor。UEの基本形状と既存の `M_NinjaBurst` を使い、閃光1、火球10、煙10、火花24、衝撃波24の計69コンポーネントを動かす。影・衝突・ナビゲーションへの影響を無効化し、寿命終了で削除する。新たな画像・コード・素材のダウンロードは行わない。

敵の `TryDefeat`、主人公の接触ダメージ・落下ペナルティによる体力0から呼ぶ。リトライ時に `Clear` で演出を削除する。爆発の位置は戦闘不能になった時点の座標なので、深い落下の場合は画面外になることがある。

爆発音は `setup_explosion_audio.py` で決定的なノイズと低音の正弦波を合成したオリジナル。元WAVは `Content/Ninja/AudioSource/S_Explosion.wav`、UE音源は `Content/Ninja/Audio/S_Explosion.uasset`。44.1kHz・16bit・モノラル。[生成・波形検証](validation/explosion-audio.json)。聞こえ方の好みやBGMとの音量バランスは試遊で調整する。

## 再現・検証

プロジェクトルートで実行する。承認済みのUE 5.8・MSVC・SDKと通常の設定・キャッシュ等を使用し、追加の外部アクセスは不要。

```powershell
& Tools/Editor/Build-Step1.ps1
& Tools/Editor/Setup-Explosion.ps1
& Tools/Editor/Verify-Explosion.ps1 -City -RunName explosion-check-new
& Tools/Editor/Verify-Step8.ps1 -City -RunName explosion-match-new
& Tools/Editor/Verify-RoundMusic.ps1 -City -RunName explosion-music-new
```

専用検証は敵AIを停止し、撃破・体力・寿命・表示・リトライを分離して調べる。撮影用カメラは検証時のみ使用し、通常の三人称カメラは維持する。SEを無効化せず起動する。音源のピークは23562/32767で、無音・クリッピングがないことを確認した。

池袋マップの専用検証9項目、従来のゲーム進行25項目、実際のM4Aデコードを伴う音楽の検証12項目が成功。曲終了試験は末尾にシークし、実際の終了イベントを確認する。全曲を省略せず再生した検証は [BGMの記録](round-music.md) を参照。

レポート: [爆発](validation/explosion-city.json)、[ゲーム進行](validation/explosion-match.json)、[BGM](validation/explosion-music.json)。

確認画像: [敵の撃破](validation/explosion-enemy.png)、[主人公の戦闘不能](validation/explosion-player.png)。撮影は従来の300秒規則を明示指定した検証モードで行い、通常プレイの制限時間は約4分4秒のBGMに従う。

同時に多数の爆発が出た場合の性能計測、Windows配布パッケージでの動作確認はStep 9〜10で行う。
