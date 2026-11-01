# Macでの開発

## 1. 環境をそろえる

Epic Games LauncherからMac用 **UE 5.8** をインストールし、フル版Xcodeを起動して初回セットアップを完了します。XcodeのCommand Line Toolsの選択先をフル版Xcodeに合わせてください。Apple Silicon Macを想定した手順ですが、実機確認はまだ行っていません。

手元のUE 5.8の `Engine/Config/Apple/Apple_SDK.json` はXcode推奨値 `26.1.1`、ツールチェーン判定範囲 `15.2.0〜27.9.0` を記録しています。この範囲は動作保証ではありません。Mac側にインストールしたUEの要件と、使用するmacOS・Xcodeの組み合わせを優先してください。

## 2. クローンと依存物

```sh
git clone https://github.com/fujinochan/Ikebukuro.git
cd Ikebukuro
python3 Tools/setup_project.py --download
```

Python 3は標準ライブラリだけを使用します。`python3` が使えない場合はUEに付属するPythonなど、インストール済みPython 3の実行ファイルで置き換えてください。スクリプトの無指定実行はネットから取得しません。公式ZIPを別端末から `Downloads/CesiumForUnreal-58-v2.30.0.zip` にコピーしてから `python3 Tools/setup_project.py` としても復元できます。

約1.26GBのZIPを保存し、全プラットフォーム向けプラグインを展開するため、数GB以上の空き容量を確保します。エンジン・ビルドキャッシュには別途空き容量が必要です。公式ZIPはMacの `Binaries/Mac` と `Darwin-universal-Release` を含みます。

## 3. ビルドと起動

UEの配置が標準と違う場合のみ環境変数を設定します。

```sh
export UE_ENGINE_ROOT="/Users/Shared/Epic Games/UE_5.8"
bash Tools/Mac/build.sh
bash Tools/Mac/open.sh
```

ビルドログは `Saved/Logs/Mac-Build.log`、起動ログは `Saved/Logs/Mac-Editor.log` です。Xcodeプロジェクトの生成は上記コマンドでは不要です。UEまたはCesiumの更新によってモジュール不整合が出た場合は、両端末のバージョンを合わせてください。Windowsの `Binaries/` はMacにコピーしません。

メインマップは `/Game/Ninja/Maps/L_Ikebukuro`。Play後に街の表示を待ち、Enterで開始します。ロード後に建物・道路、地面の色、黒忍者・赤忍者、3段ジャンプ、手裏剣、爆発、BGM・ポーズ・リトライ・曲終了時のゲームオーバーを確認してください。

MacのBGMは `AvfMedia`、Windowsは `WmfMedia` を使用します。Mac用メディアプラグインはUE標準で有効です。ローカル3D TilesのURLはWindowsのドライブパスとMacのPOSIXパスの両方から生成します。開発支援用のローカルMCP/EditorToolsetプラグインへの必須依存は外しています。

## 4. 変更を共有する

作業開始前に `git pull --ff-only` し、作業後は変更をコミットして `git push` します。`.umap` や `.uasset` の同時編集を避けてください。競合時にバイナリをテキストマージしないでください。

今回のWindows環境ではMac向けC++コンパイル・Metal描画・AVFoundationによる音声再生は検証できません。Macで初回ビルド・Playの確認が必要です。Windows用のパッケージ作成スクリプトは `Tools/Editor/Package-Step10.ps1` にあります。Mac向けShippingビルド・配布ZIP・署名と公証の手順は [Mac向け移植・配布](mac-distribution.md) を参照してください。
