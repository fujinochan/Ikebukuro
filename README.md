# Ikebukuro Ninja City

Unreal Engine 5.8で作る三人称の忍者ミニゲーム。池袋の実際の3D都市モデルを走り、3段ジャンプ・踏みつけ・直進する手裏剣で赤忍者を倒します。黒忍者は一撃で終了し、BGMが終わるまでに敵を全滅させるとクリアです。

## 開発を始める

必要なものは **Unreal Engine 5.8**、対応するC++開発環境、Git、Python 3です。WindowsはVisual Studio/MSVC、Macはフル版Xcodeを用意してください。

```sh
git clone https://github.com/fujinochan/Ikebukuro.git
cd Ikebukuro
python3 Tools/setup_project.py --download
```

`--download` はCesium for Unreal v2.30.0の公式ZIP（約1.26GB）を `Downloads/` に取得し、固定したSHA-256を照合して `Plugins/` に展開します。このZIPにはMacのバイナリも含まれます。既存のプラグインは上書きしません。インストール済みなら `python3 Tools/setup_project.py --check` で必要データを検証できます。Windowsでは `python3` を利用可能なPython実行ファイルに読み替えてください。

プレイ用に切り出した建物・道路の3D Tiles（約66MB）、加工済み地面テクスチャ、BGMはこのリポジトリに含みます。元の豊島区全域ZIP、Cesium本体、エンジン、ビルド成果物は含みません。Git LFSは不要です。地図の実行時読み込みはオフラインです。

### Mac

[Macでの開発手順](doc/mac-development.md)と[Mac向け配布手順](doc/mac-distribution.md)を参照してください。

```sh
bash Tools/Mac/build.sh
bash Tools/Mac/open.sh
```

### Windows

```powershell
powershell -ExecutionPolicy Bypass -File Tools/Editor/Build-Step1.ps1
```

`IkebukuroNinjaCity.uproject` をUE 5.8で開き、`Content/Ninja/Maps/L_Ikebukuro` をPlayしてください。エディタを閉じてからビルドします。

## 操作

| 操作 | キー |
| --- | --- |
| 移動・視点 | WASD・マウス |
| 走る | Shift |
| 3段ジャンプ | Spaceを頂点付近で押し直す |
| 手裏剣 | F / 左クリック |
| 試合開始・リトライ | Enter |
| ポーズ | P / Esc |
| 路面に戻る | R |
| 終了 | Q（開始・結果画面ではEscも可） |

## 共同開発

同じUEバージョンとCesiumバージョンを使用してください。C++・設定は通常のGitで管理し、`.uasset`・`.umap` は同じファイルを同時編集しないよう担当を分けます。WindowsのDLLやMacのdylib、キャッシュ、生成したIDEプロジェクトをコミットしないでください。

詳細な計画・実装記録は [doc/](doc/)、素材の出典・加工内容・利用条件は [CREDITS](Content/Distribution/CREDITS.md) と [Licenses](Content/Distribution/Licenses/) にあります。UEテンプレート素材・地図・BGMなどはそれぞれの利用条件に従います。リポジトリ全体に一括のオープンソースライセンスは付与していません。
