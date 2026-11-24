# Nickey44A-PAD ダイヤル版 (ブランチ tps43-dial)

左手のトラックパッドを外し、sekigon hires-dial-module (PAT9125) に置き換えた構成。
右手のトラックパッドは tps43 ブランチと同じ (docs/dual-tps43.md の「右手」部分)。

## 配線 (ピッチ変換FPC、パッド6 = 四角パッド)

| FPC | 信号 | 接続先 |
|---|---|---|
| 1 | GND | 基板パッド1 (GND) |
| 2 | SDA | 基板パッド2 = XIAO P0.09 (NFC1) |
| 3 | SCL | 基板パッド3 = XIAO P0.10 (NFC2) |
| 4 | PUSH | XIAO P0.16 (白線) 押すと LOW |
| 5 | MOTION | XIAO P1.10 (黄線) アクティブ LOW |
| 6 | 3.3V | 基板パッド6 (3.3V) |

I2C プルアップはモジュールにないので、XIAO の内部プルアップを使う。アドレス 0x75。

## ファームウェアの構成

- 左 (peripheral): `sekigon,hires-dial` が回転を読み、ZMK のセンサー転送で右へ送る。
  押し込みは `zmk,kscan-gpio-direct` を `zmk,kscan-composite` でマトリクスの RC(3,5)
  (左の内側キー、キーマップ 30 番目) に重ねている。
- 右 (central): `sekigon,remote-hires-dial` が受け取り、キーマップの `sensor-bindings` を実行。
  `counts-per-revolution` と `triggers-per-rotation` は左右で同じ値にすること。

## 割り当て

押し込みは全レイヤーで中クリック。

| レイヤー | 回す |
|---|---|
| BASE | 縦スクロール (右回り=下、左回り=上) |
| SYMBOL | 拡大縮小 (Ctrl+ホイール、右回り=拡大) |
| FUNC_NUM | 音量 (右回り=上げる) |
| ARROW | タブ切替 (右回り=次のタブ Ctrl+Tab) |

## 調整

- 回転方向が全部逆: `nickey44a_l.overlay` の `invert;` のコメントを外す。
- 音量・タブの 1 周あたりの回数: 左右 overlay の `triggers-per-rotation` (今は 24)。
- スクロール・拡大縮小の速さ: キーマップの `&hires_dial_scroll 60 1` / `&dial_zoom 150 1`
  (最初の数字を小さくすると速くなる)。
