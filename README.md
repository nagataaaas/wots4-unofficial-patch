# unofficial_patch

[English](README.en.md)

> [!NOTE]
> この Mod は [Steam 版『侍道4』](https://store.steampowered.com/app/312780/4/?l=japanese)専用です。

『侍道4』の非公式パッチです。クラッシュやバグを直し、プレイ体験を向上することが目的です。

## 機能

次の問題を直します。修正ごとの詳しい説明（どんな問題か・なぜ起きるか・考えた対策・採った対策・備考）は [`docs/fixes/`](docs/fixes/) にあります。

1. 牢屋の写真家から再開すると、真ルートの結末（鬼怒川を倒した直後）でクラッシュする問題（[詳細](docs/fixes/jail_ending_crash.md)）
2. 「士魂は屈せず」でまだらに助けられたとき、取り上げられた武器・アイテム・服が返ってこない問題（[詳細](docs/fixes/madara_rescue_property.md)）
   - まだらに助けられる流れは通常のプレイにはなく、セーブエディタなどでフラグを書き換えたときにしか起きません。
   - すでにこの状態で保存したセーブの武器は戻せません。
3. 被写界深度（DoF）設定が有効なとき、光（bloom）や蹴ったときのエフェクトなどに、白や黒の四角が付く問題（[詳細](docs/fixes/effect_box.md)）
4. 全画面で、ウィンドウに戻るたびに画面が白くなって一瞬止まり、ディスプレイによっては画面の外まで広がって見える問題（[詳細](docs/fixes/focus_white_screen.md)）
   - Windows の拡大率が 100% でないときに起きていました。
5. 夜這いのあと、セーブせずに「タイトルに戻る」から夜這いの約束があるセーブをロードすると、瞬時に夜這い終了時の処理が走り約束が消える問題（[詳細](docs/fixes/yobai_result_after_title.md)）
6. ステレオで聞くと、フィールドで主人公の近くの声や効果音がほとんど聞こえず、カメラの後ろの音だけが聞こえる問題（[詳細](docs/fixes/listener_orientation.md)）
   - Windows Sonic などの空間音響をオンにしなくても聞こえるようになります。
7. 台詞やアイテム説明の全角数字が、黒い四角として表示される問題（[詳細](docs/fixes/fullwidth_digits.md)）
   - 入力の再割り当て画面やチュートリアルの数字キー画像は維持します。数字キー画像の表示は要ゲーム内確認です。
8. 俺流や刀の名前の入力で、IME で「え」「け」などを打とうとすると E キーで入力が閉じてしまい、日本語の文字も入ったり入らなかったりする問題（[詳細](docs/fixes/text_input.md)）
   - 入力中はキーボードのキーがボタン（E＝戻る、スペース＝決定）として働かなくなります。キーボードでは Enter で決定、Esc で戻ります（主人公の名前の入力と同じです）。
   - 主人公の名前に、鬼・郎・龍などの一部の漢字や全角英数を入れられない問題もあわせて直します。
9. 拷問や捕縛のあと、釣り成功の効果音や店の人などの声が鳴らなくなり、メニューを開け閉めすると拷問の効果音が鳴るようになる問題（[詳細](docs/fixes/sound_slot_leak.md)）
10. タイトル画面でオプションを開いた後に「はじめから」を選んだり、周回を終えたセーブを「途中から」でロードしたりすると、クラッシュすることがある問題（[詳細](docs/fixes/title_select_window.md)）

現状予定している追加修正はありません。
修正を希望する不具合がある場合は、このリポジトリの Issue に起票していただければ対応するかもしれません。

## インストール

### 自動インストール（推奨）

1. ゲームを終了してください。
2. [WayOfTheSamurai4ModInstaller.exe](https://github.com/nagataaaas/wots4-mods/releases/latest/download/WayOfTheSamurai4ModInstaller.exe) をダウンロードして実行してください。
   - 「Windows によって PC が保護されました」と出たら、「詳細情報」→「実行」を押してください。
   - 管理者として実行してよいかの確認が出たら、「はい」を押してください（ゲームのフォルダに書き込むためです）。
3. 画面の案内に沿って進め、Mod の一覧で `unofficial_patch` を選んでください。↑↓ で移動し、Space で選び、Enter で確定します。

ModLoader も一緒に入ります。更新も同じ手順でできます。設定（`config.ini`）は引き継がれます。

<details>
<summary>手動インストール</summary>

> [!WARNING]
> 事前に [wots4-mods（ModLoader）](https://github.com/nagataaaas/wots4-mods) を導入してください。

1. ゲームを終了してください。
2. [Releases](../../releases) から `wots4-mod-unofficial-patch-<バージョン>.zip` をダウンロードし、解凍してください。
3. 解凍して得られた `mods` フォルダを、侍道4のインストールフォルダ（`WayOfTheSamurai4.exe` がある場所）へコピーしてください。すでに `mods` フォルダがある場合は、その中にまとめてください。
4. `{侍道4インストールフォルダ}/mods/unofficial_patch/mod.dll` があることを確認し、ゲームを起動してください。

更新するときに今の設定を残したい場合は、`mods/unofficial_patch/config.ini` を上書きせずに、`mod.dll` だけを置き換えてください。

</details>

## アンインストール

1. ゲームを終了してください。
2. `{侍道4インストールフォルダ}/mods/unofficial_patch/` フォルダを削除してください。中の設定（`config.ini`）も消えます。

## 設定

ゲームを終了し、`{侍道4インストールフォルダ}/mods/unofficial_patch/config.ini` を編集してください。保存してからゲームを起動すると、設定が反映されます。

修正ごとにセクションがあり、`enabled` が `1` ならその修正を入れ、`0` なら入れません（ゲームの元の動きのままです）。`1` と `0` 以外の値は `1` として扱います。ファイルや項目がないときは、すべての修正を入れます。

| セクション | 項目 | 既定値 | 説明 |
| --- | --- | --- | --- |
| `follow_detach` | `enabled` | `1` | 1. 牢屋の写真家から再開したときの結末のクラッシュを直します。 |
| `property_return` | `enabled` | `1` | 2. まだらに助けられたときに持ち物が返らない問題を直します。 |
| `translucent_depth` | `enabled` | `1` | 3. エフェクトに白や黒の四角が付く問題を直します。 |
| `physical_screen_check` | `enabled` | `1` | 4. ウィンドウに戻るたびの白画面と停止を直します（解像度の判定）。 |
| `skip_full_screen_reapply` | `enabled` | `1` | 4. ウィンドウに戻るたびの白画面と停止を直します（全画面のかけ直しの省略）。 |
| `no_white_background` | `enabled` | `1` | 4. ウィンドウに戻るたびの白画面と停止を直します（背景を白で塗らない）。 |
| `yobai_result_reset` | `enabled` | `1` | 5. タイトルに戻ったあとにロードすると夜這いの約束が消える問題を直します。 |
| `listener_orientation` | `enabled` | `1` | 6. ステレオで主人公の近くの音が聞こえない問題を直します。 |
| `fullwidth_digits` | `enabled` | `1` | 7. 全角数字の表示を直し、操作案内の数字キー画像を維持します。 |
| `text_input_keyboard` | `enabled` | `1` | 8. 俺流や刀の名前の入力中に、E やスペースで入力が閉じないようにします。 |
| `text_input_characters` | `enabled` | `1` | 8. 俺流や刀の名前に日本語を入力できるようにします。 |
| `player_name_characters` | `enabled` | `1` | 8. 主人公の名前に、一部の漢字や全角英数を入力できるようにします。 |
| `sound_slot_release` | `enabled` | `1` | 9. 拷問のあとなどに、効果音や声が鳴らなくなる問題を直します。 |
| `title_select_window` | `enabled` | `1` | 10. タイトル画面でオプションを開いた後のクラッシュを直します。 |

## ビルド

> [!NOTE]
> この Mod を利用するだけであれば不要です。修正や開発を行いたい場合にのみ参照してください。

ふつうは [wots4-mods](https://github.com/nagataaaas/wots4-mods) の中でビルドします。この Mod だけをビルドするときは、`-DWOTS4_SDK_DIR=<wots4-mods のパス>` を付けて CMake を実行します。
