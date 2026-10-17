# BGMの終了を制限時間にする

作業日: 2026-10-17。Step 8への追加要望として実装。

## 遊び方

池袋マップ `L_Ikebukuro` でEnterを押し、3秒のカウントダウンが終わって操作可能になる瞬間に `Ikebukuro Ninja City.m4a` を先頭から再生する。既存の効果音も継続する。曲はループしない。

- 曲が終わるまでに赤忍者8体を倒せば勝利。曲の終了時に敵が残っていればゲームオーバー。
- 曲の長さは実測243.719秒（約4分4秒）。HUDは音楽の再生位置から残り時間を表示する。都市マップの従来の300秒制限を置き換える。
- Pでポーズすると音楽と残り時間が停止し、再開すると同じ位置から続く。
- 勝利・体力0による敗北・マップ終了で音楽を止める。Enterでリトライすると曲も先頭に戻る。
- 最後の敵の撃破と曲の終了が同じフレームなら勝利を優先する。
- 音楽が開けないときは開始せず、エラーとEnterによる再試行を表示する。

音量はBGMの倍率0.35。主観的なSEとのバランスは試遊で調整する。

## 音源と実装

ユーザー提供のプロジェクトルートのM4Aを、内容を変換せず `Content/Movies/Ikebukuro Ninja City.m4a` にコピーした。元ファイルは変更しない。SHA-256は `8ec90ccc14e5531c09e091738efd9874beb1cc988408d5ebbf2067480a5838fd`、3,864,282 bytes。[元音源との一致検証](validation/round-music-source.json)。

UE 5.8に同梱されるWindowsのWmfMedia、MediaPlayer、MediaSoundComponentを使用。準備中に音源を開き、実際に報告された長さと再生位置を使う。終了判定はメディアの終了イベントで行い、固定秒数から推測しない。追加ツールやダウンロードはない。既存のStep 8のUE・MSVC・SDKと通常キャッシュ等への承認範囲でビルド・検証した。

`DefaultGame.ini` でMoviesをNonUFS同梱対象に指定した。Windows配布パッケージでの実動作確認はStep 10で行う。既存の練習マップは300秒の進行を維持する。

## 再現手順

プロジェクトルートで、承認済みのUE環境を使う。検証名は既存レポートと重複しない値にする。

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' Tools/Editor/setup_round_music.py
& Tools/Editor/Build-Step1.ps1
& Tools/Editor/Verify-RoundMusic.ps1 -City -RunName music-city-new
& Tools/Editor/Verify-RoundMusic.ps1 -FullTrack -RunName music-full-new
& Tools/Editor/Verify-RoundMusic.ps1 -MissingFile -RunName music-missing-new
& Tools/Editor/Verify-Step8.ps1 -City -RunName music-regression-new
```

音楽検証は実際のWMFデコーダーと音声ミキサーを使い、固定物理時間やNoSoundを使わない。AIだけを停止して音楽のライフサイクルを検証する。`-FullTrack` は最初の曲を途中へシークせず最後まで再生するため、約4分以上かかる。通常の短い検証は曲末尾へシークして本物の終了イベントを試す。

Step 8の従来の進行・経路検証には `-NinjaLegacyTimerCheck` を明示し、300秒の既存規則を回帰検証する。曲による時間制限は別の音楽検証で確認する。

## 検証記録

UE Editor対象をビルド済み。実際の池袋マップで12項目成功し、無音でないデコード、開始・ポーズ・曲末尾による敗北・2回のリトライ・体力0・曲終了と最後の撃破の同時発生を確認した。別の試験で曲の途中の勝利による停止も確認した。音源欠落時の開始ブロックとエラー表示、および従来の池袋ゲーム進行25項目も成功。

約243.719秒の曲をシークせず最後まで再生し、自然終了による敗北を確認した。ポーズ約0.8秒を含む経過時間は244.554秒。全曲検証は3体の練習マップに音楽を明示有効化して行い、通常描画の都市マップ8体版は曲末尾へのシークで検証した。

レポート: [全曲再生](validation/round-music-full.json)、[池袋8体版](validation/round-music-city.json)、[途中の勝利](validation/round-music-early-win.json)、[音源欠落](validation/round-music-missing.json)、[既存25項目](validation/round-music-legacy.json)。

曲終了時の画面: [確認画像](validation/round-music-city.png)。
