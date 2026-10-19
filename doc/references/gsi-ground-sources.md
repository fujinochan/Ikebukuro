# 地面色に使用した国土地理院の資料と写真

2026-10-19、ユーザーが公式資料・池袋の航空写真の取得、加工、ゲームへの同梱を承認。

| 保存した資料 | 取得元 |
| --- | --- |
| `gsi-tile-list.html` | https://maps.gsi.go.jp/development/ichiran.html |
| `gsi-tile-spec.html` | https://maps.gsi.go.jp/development/siyou.html |
| `gsi-content-use.html` | https://www.gsi.go.jp/kikakuchousei/kikakuchousei40182.html |

資料原本のSHA-256は `gsi-reference-hashes.json`。読み取り用のテキストも保存した。HTML原本を優先する。

公式タイル一覧は、写真 `seamlessphoto`（ズーム14〜18）を「基本測量成果以外で出典の記載のみで利用可能なもの」の節に掲載している。取得時のコンテンツ利用規約は特記のないコンテンツに公共データ利用規約1.0を適用し、出典・加工表記を求めている。

写真は公式一覧に記載された `https://cyberjapandata.gsi.go.jp/xyz/seamlessphoto/{z}/{x}/{y}.jpg` から取得した。ズーム17、池袋のプレイ範囲と150mの周囲を覆う12枚で、原本は `Downloads/GSI/seamlessphoto-z17/` に保存。個別のURL・バイト数・SHA-256は `Downloads/GSI/download-record.json`。地図の原点と範囲は既存の `Tools/City/ikebukuro-area.json` を使用した。

公式一覧には一部地域の追加出典条件もある。GRUS画像として記載された追加表記対象の範囲は沖ノ鳥島付近であり、この池袋の取得範囲には該当しない。撮影年月は今回特定しておらず、取得日と区別する。

派生画像の地面色には「地理院タイル 全国最新写真（シームレス）を加工して作成」の出典・加工内容をゲーム同梱の `Content/Distribution/CREDITS.md` に記載した。写真取得と処理はオフライン用の作成工程に限り、完成ゲームで写真サーバーへアクセスしない構成とする。
