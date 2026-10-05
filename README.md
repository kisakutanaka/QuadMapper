# QuadMapper

Syphon (macOS) / Spout (Windows) の映像を 4 隅のホモグラフィ変換で出力するマッピングツール。

## ビルド

CMake 3.20 以上。依存 (GLFW, Dear ImGui, Syphon, Spout2) は configure 時に取得される。

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build
```

- macOS: `build/QuadMapper.app`。Universal にするなら `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`。
- Windows (Visual Studio): `build/Release/QuadMapper.exe`。ランタイム静的リンクの単体 exe。

## 操作

- Source でテストパターンか Syphon・Spout の送信元を選ぶ。テストパターンは `patterns/` 内の PNG/JPEG (ファイル名順)。追加・削除してビルドするとアプリに反映される
- 出力は 2 つまで (Add output / Remove output)。GUI で選択中の出力を編集する。2 出力は同じ受信フレームから描画される
- 2 台のプロジェクタでのブレンド例 (重なり 10%): 出力 1 は crop x1=0.55・blend R=0.18、出力 2 は crop x0=0.45・blend L=0.18 (blend 幅はその出力の担当範囲に対する割合)
- 出力ごとにソースを回転 (Rotation: 時計回り 0/90/180/270) ・反転 (Flip H / Flip V、出力上の向き) できる
- 出力窓またはプレビュー上で緑 (隅) / 赤 (マスク頂点) の点をドラッグ
- 出力窓で Esc: フルスクリーン解除
- 矢印キーで選択点を 1px (Shift で 10px) 移動、Tab で隅を順に選択
- マスクは多角形の内側を隠す。Insert point で選択頂点の次に点を追加
- 設定は終了時に自動保存、起動時に復元
  (アプリの隣の `QuadMapper.json`。mac は .app と同じフォルダ、Windows は exe と同じフォルダ)
- File 欄の相対パスはアプリと同じフォルダが基準 (既定 `QuadMapper.json`)

## 構成

- `src/core` OS・GL 非依存 (ホモグラフィ、設定)
- `src/render` OpenGL 描画
- `src/platform` OS 依存部 (`Platform.h` の実装: mac/Syphon, win/Spout)

## 受信テスト (macOS)

`build/SyphonTestSender [width height]` でタイムコード (HH:MM:SS:FF, 60fps) と流れるバーを Syphon 送信する
(既定 1920x1080)。QuadMapper の Source に `SyphonTestSender - Timecode` として出る。
