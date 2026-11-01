# Step 1 — 起動・ビルド・テスト用マップの準備

実施日: 2026-10-16（日本時間）

状態: 開発基盤の準備と、エディタでの動作確認を完了。

## 作成・設定したもの

- 変更前のプロジェクトを `Backups/step1-before-setup-20261016.zip` に保存した。
- ローカル Git リポジトリを作成し、変更前の内容を baseline としてコミットした。リモートは設定していない。
- `.gitignore` と `.gitattributes` を追加した。生成物、バックアップ、原本の都市データZIPは履歴の対象から除外した。
- `Content/Ninja/` に Blueprints、Input、Maps、Materials、UI を用意した。
- `Content/Ninja/Maps/L_TestArena.umap` を作成した。
- エディタの起動マップを `L_TestArena` に設定した。
- Hardware Targeting を Desktop に設定した。初期の検証対象は Windows PC とする。
- フォルダアクセスとダウンロードに関するガードレールを `AGENTS.md` に記録した。

テストマップは一辺100mの床、50cm・1m・2mの段差、今後のジャンプ調整用の高さ10mの箱、PlayerStart、ライトで構成する。ゲームモード・主人公・カメラは既存の Third Person Blueprint を使う。忍者の専用機能は後続ステップで追加する。

## 確認した環境

| 項目 | 結果 |
| --- | --- |
| Unreal Engine | 5.8.2 / CL 56702186 |
| UE の場所 | `C:\Program Files\Epic Games\UE_5.8` |
| ビルド対象 | `IkebukuroNinjaCityEditor Win64 Development` |
| Visual Studio | `C:\Program Files\Microsoft Visual Studio\18\Professional` |
| MSVC | 14.51.36260（ツールチェインフォルダ 14.51.36231） |
| Windows SDK | 10.0.26100.0 |
| エディタ用 Python | UE 同梱の 3.11.8。実行時だけ PythonScriptPlugin を有効化 |
| Git | インストール済み `C:\Program Files\Git\cmd\git.exe` を使用、ブランチ `main` |

## 確認結果

### ビルド

UnrealBuildTool の結果は `Succeeded`。既存のコンパイル済み内容を利用する増分ビルドであり、全ソースを強制的に再コンパイルするクリーンビルドは行っていない。

- 詳細: `Saved/Logs/Step1-Build-Retry.log`
- 再実行用: `Tools/Editor/Build-Step1.ps1`

最初の試行では UBA が標準の `C:\ProgramData\Epic\UnrealBuildAccelerator` へ書き込めず停止した。`-UBARootDir` で `Saved/UnrealBuildAccelerator` を指定して解消した。UE 5.8 では `-NoUBA` を指定しても UBA 実行基盤自体を使うため、保存先指定が必要だった。

### Play の確認

実際にエディタを起動し、PIE（Play In Editor）でキャラクターの操作処理を呼び出して測定した。

| マップ | 水平移動 | 視点Yawの変化 | ジャンプ上昇量 | 着地 |
| --- | --- | --- | --- | --- |
| ThirdPerson / Lvl_ThirdPerson | 330.43cm | 37.5度 | 127.55cm | 成功 |
| Ninja / L_TestArena | 334.77cm | 37.5度 | 127.53cm | 成功 |

- 自動確認: `Tools/Editor/verify_step1.py`
- 結果: `Saved/Validation/step1-play-check.json`（`status: passed`）
- ログ: `Saved/Logs/Step1-PlayCheck-Retry.log`
- マップ生成結果: `Saved/Validation/step1-map-setup.json`

これは `DoMove`、`DoLook`、`DoJumpStart` / `DoJumpEnd` を通した確認。物理キーボード入力、マウス入力、描画品質や操作感の目視評価は未実施。次にエディタで試遊するときに確認する。

## 自分で試す手順

1. `IkebukuroNinjaCity.uproject` を UE 5.8 で開く。
2. `L_TestArena` が開くことを確認する。必要なら Content Browser の `Ninja/Maps/L_TestArena` を開く。
3. Play を押して画面内をクリックする。
4. WASDで移動、マウスで視点変更、Spaceで通常ジャンプする。
5. 床に立てること、段差での動き、ジャンプ後に着地できることを確認する。
6. EscでPlayを終了する。

現在は通常の Third Person 操作。走行の切り替え、3段ジャンプ、手裏剣は実装前。

## 再現用スクリプト

- `Tools/Editor/Build-Step1.ps1`: 既存環境でビルドする。UEとコンパイラへのアクセス承認が必要。
- `Tools/Editor/setup_step1.py`: UE PythonScript commandletでテストマップを作成する。既存のテストマップがある場合は上書きしない。
- `Tools/Editor/verify_step1.py`: 専用のエディタプロセスで2つのマップをPIE検証し、レポートを書いてそのプロセスを終了する。

Play確認の起動例（プロジェクトルートで実行）:

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  "$PWD\IkebukuroNinjaCity.uproject" `
  /Game/ThirdPerson/Lvl_ThirdPerson `
  "-ExecutePythonScript=$PWD\Tools\Editor\verify_step1.py" `
  -EnablePlugins=PythonScriptPlugin `
  -DisablePlugins=ModelContextProtocol,Fab,EOSShared,EditorTelemetry `
  '-ini:Engine:[HTTP]:bEnableHttp=False' `
  -unattended -nop4 -nosplash -NoSound -NoAnalytics -NoEpicPortal `
  -windowed -ResX=1280 -ResY=720 `
  "-abslog=$PWD\Saved\Logs\Step1-PlayCheck.log"
```

確認処理は非同期で進むため、エディタプロセスの終了とレポートの `status` の両方を確認する。終了コードだけで成功と判断しない。

## 既知の警告と対応

- MSVC 14.51 は UE が推奨する 14.50 より新しいという警告がある。今回の増分ビルドとPlayは成功した。今後コンパイラ依存の問題が出た場合に再確認する。
- 既存ログと今回のエディタ起動ログに、起動時の `LogAutomationTest: Error: Condition failed` が複数ある。今回実行したPIE検証のアサーションではなく、既存環境でも発生しているログ。原因は未特定であり、今回の `passed` は専用のPlay確認レポートを指す。
- 最初のマップ生成は既存 ModelContextProtocol プラグインの localhost:8000 バインドエラーにより、commandlet の終了コードが1になった。Pythonの実行とマップ保存は成功しており、後続のPlayでマップを実際に読み込んで確認した。Playの再確認ではこのプラグインをコマンドラインで無効化した。
- 制限された実行環境ではUEがログ生成前に終了したため、ユーザー承認済みの範囲で制限外のUE実行を使った。

## バックアップと履歴

バックアップ対象: `Config/`、`Content/`、`Source/`、当時の `doc/`、`.uproject`、`.vsconfig`。エディタ生成物と原本地図ZIPは含めていない。原本地図ZIPはプロジェクトルートに保持している。

- ZIP: `Backups/step1-before-setup-20261016.zip`
- SHA-256: `23C53AA5F1F6B0F0B9FFC95E746D93DD973715CB12CA013783194D502A3C2EC4`
- 変更前のローカルコミット: `1e4fdf4`（`chore: preserve existing Unreal project baseline`）
- コミット作成者はローカル作業用 `Codex local setup <codex@localhost>`。ユーザー個人のGit設定を参照していない。

復元時はZIPをプロジェクト内の別ディレクトリへ展開し、必要な差分を確認してから戻す。

## アクセスの承認記録

この作業では、まずプロジェクト内だけを調査した。その後、会話で以下の明示承認を得た。

1. Step 1 のため、外部にインストールされた UE 5.8、Visual Studio / MSVC、Windows SDK を読み取り・実行し、各ツールの通常の設定・キャッシュ・一時ファイルを読み書きすること。
2. インストール済み Git の場所の確認と実行。リポジトリはローカルに閉じ、リモート接続しないこと。

Gitはシステム・ユーザー設定の読み込みを無効化し、プロジェクト内の空設定・空テンプレート・無効化したhooksを指定して初期化した。外部素材・コードの取得は行っていない。追加の外部アクセスやインターネット上の素材取得は、その都度承認を得る。

次の計画上の工程は Step 2「地図読み込みの小規模実証」。地図プラグインの取得が必要な場合は、対象と保存先を示して先に承認を得る。

後続工程の承認追記（2026-10-16）: Step 5でも、同じUE 5.8・Visual Studio / MSVC・Windows SDKの読み取り・実行と、通常の設定・キャッシュ・一時ファイルへのアクセスをユーザーが明示承認した。追加のダウンロードは行っていない。実装と検証は [Step 5の記録](step5-combat.md) を参照。

後続工程の承認追記（2026-10-16）: Step 6でも、同じUE 5.8・Visual Studio / MSVC・Windows SDKの読み取り・実行と、通常の設定・キャッシュ・一時ファイルへのアクセスをユーザーが明示承認した。追加のダウンロードは行っていない。実装と検証は [Step 6の記録](step6-encounters.md) を参照。

後続工程の承認追記（2026-10-16）: Step 6.5でも、同じUE 5.8・Visual Studio / MSVC・Windows SDKの読み取り・実行と、通常の設定・キャッシュ・一時ファイルへのアクセスをユーザーが明示承認した。元ZIP内のLOD2を使い、追加のダウンロードは行っていない。[Step 6.5の記録](step6_5-textures.md) を参照。

後続工程の承認追記（2026-10-17）: Step 7でも、同じUE 5.8・Visual Studio / MSVC・Windows SDKの読み取り・実行と、通常の設定・キャッシュ・一時ファイルへのアクセスをユーザーが明示承認した。追加のダウンロードは行っていない。[Step 7の記録](step7-match-flow.md) を参照。

後続工程の承認追記（2026-10-17）: Step 8でも、同じUE 5.8・Visual Studio / MSVC・Windows SDKの読み取り・実行と、通常の設定・キャッシュ・一時ファイルへのアクセスをユーザーが明示承認した。追加のダウンロードは行わない。

Step 8追加BGM（2026-10-17）: ユーザー提供のプロジェクト内M4Aを使用。承認済みUEのWMF再生機能で実装・検証し、追加の外部ツールやインターネット取得は行っていない。[実装記録](round-music.md)。

後続工程の承認追記（2026-10-17）: Step 9でも、同じUE 5.8・Visual Studio/MSVC・Windows SDKの読み取り・実行と、通常の設定・キャッシュ・一時ファイルへのアクセスをユーザーが明示承認した。追加ダウンロードは行わない。[自動カメラの記録](step9-auto-camera.md)。

Step 10承認追記（2026-10-17）: UE 5.8・Visual Studio/MSVC・Windows SDKと通常の設定・キャッシュ・一時ファイルへのアクセスを会話で承認済み。配布作成と起動検証に使用する。別途承認されたPLATEAU公式サイトポリシーのみをdoc/references/へ取得した。画像・地図・コードの追加取得は行わない。

2026-10-19 地面色の承認追記: 国土地理院の公式提供状況・利用条件と、池袋のプレイ範囲および周囲の航空写真タイルの取得・加工・地面テクスチャとしての同梱を会話で明示承認された。資料はdoc/references/、写真はDownloads/GSI/へ保存する。追加の外部ツール取得は行わず、承認済みUEの画像処理機能を使う。


## 2026-10-19 GitHub / Mac共有の承認

ユーザーの指定により `https://github.com/fujinochan/Ikebukuro` へのリモート接続・Public公開を許可。従来のローカル限定Gitルールは、この共有先について上書きする。他リポジトリやユーザーGit設定へのアクセスは含まない。Cesiumは承認済みの同一公式ZIPを固定SHA-256で復元する。実行に必要な切り出し済み都市データ・加工済み地面・BGMを共有し、元ZIP、ローカルキャッシュ、プラグイン本体、ビルド成果物は除外する。Mac実機検証は未実施。

## 2026-11-01 Mac移植の追加許可

ユーザーが、インストール済みXcode／Apple SDK／署名・公証用の標準ツールと、通常の設定・キャッシュへのアクセスを許可。プロジェクト内に成果物・ログを保持する。公証サービスへのアップロードや新規ツール取得はこの許可に含めない。

確認: UE `/Users/Shared/Epic Games/UE_5.8`、Xcode `/Applications/Xcode.app`（27.2 / 27B52）。UE同梱Apple_SDK.jsonのツールチェーン判定範囲は15.2.0〜27.9.0。Cesiumプラグインと元ZIPはチェックアウトに存在しない。地図・BGMの48ファイルは `setup_project.py --check` のデータ検証に合格したが、プラグイン検証で停止。

同日、ユーザーが不足中の公式Cesium 2.30.0 ZIP（約1.26GB）の再取得と、プロジェクト内でのSHA-256検証・復元を許可。

同日、XcodeのMetal Toolchain未導入（`xcrun metal --version` がmissing Metal Toolchain）を確認し、ユーザーがApple公式Metal Toolchainの取得・標準登録を追加許可。`xcodebuild -downloadComponent MetalToolchain -exportPath <project>/Downloads/MetalToolchain` で取得する。ローカル署名検証で有効なコード署名証明書は0件。公開向けDeveloper ID署名・公証は別途設定が必要。

Metal Toolchain 27B47（約839MB）はXcodeのdownloadComponent実行時に標準領域へ導入され、同時に `Downloads/MetalToolchain/MetalToolchain-27B47.exportedBundle` へエクスポートされた。後続importComponentは「already installed」で終了したが、`xcrun metal --version` はApple metal 32023.922として成功。
