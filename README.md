# ESP32 + Google Sheet 場景同步範例

本專案提供兩份程式：
1. `ESP32_GoogleSheet_Controller.ino`：ESP32 透過 WiFi 與 GAS 溝通。
2. `GoogleAppsScript_Code.gs`：GAS 操作 Google Sheet `Sheet1` 與 `Sheet2`。

## 需求對應
- Arduino IDE `1.8.19`
- ESP32 Core `1.0.6`
- Google Sheet ID 已配置為 `XXXXXXXXXXX`（可直接替換）
- Sheet1 欄位：`Scene,r,w,b,fr,ser,sew,seb,sefr`
- Sheet2 欄位：`Operation Date and Time,Function,Scene,r,w,b,fr,ser,sew,seb,sefr`
- 支援 FUNCTION：`UPDATE`,`DELETE`,`ADD SCENE`,`UPDATE ALL`,`DELETE ALL`
- ESP32 EEPROM 最多存 10 組場景（Scene 1~10）

## ESP32 序列埠指令
請在 Arduino Serial Monitor 設定「Newline」，輸入下列指令：

- 讀取雲端資料：
  - `READ`
- 新增場景（會以 `ADD SCENE` 寫入 GAS）：
  - `ADD <scene> <r> <w> <b> <fr> <ser> <sew> <seb> <sefr>`
- 更新單筆（`UPDATE`）：
  - `UPDATE <scene> <r> <w> <b> <fr> <ser> <sew> <seb> <sefr>`
- 刪除單筆（`DELETE`）：
  - `DELETE <scene>`
- 更新所有本機場景到雲端（`UPDATE ALL`）：
  - `UPDATE_ALL`
- 刪除所有本機與雲端場景（`DELETE ALL`）：
  - `DELETE_ALL`

## 執行流程
1. ESP32 開機後先連接 WiFi
   - SSID: `TYNRICH`
   - PASSWORD: `123456`
2. 連線成功後呼叫 GAS `READ`，抓取 Sheet1 的資料。
3. 在 Serial Monitor 輸出：
   - `場景編號,r,g,b,fr,ser,seg,seb,sefr`
4. 後續透過 Serial 指令操作，ESP32 會同步更新 EEPROM 與 Google Sheet。
5. GAS 每次異動都會寫入 Sheet2，包含時間、Function 與資料內容。

## GAS 部署提醒
1. 將 `GoogleAppsScript_Code.gs` 貼到 Apps Script。
2. 確認常數：
   - `SPREADSHEET_ID`
   - `SHEET_LATEST = "Sheet1"`
   - `SHEET_LOG = "Sheet2"`
3. 部署為 Web App（Execute as: Me, Who has access: Anyone）。
4. 將 Web App URL 填入 `ESP32_GoogleSheet_Controller.ino`：
   - `GAS_WEB_APP_URL`

## 注意
- `ESP32_GoogleSheet_Controller.ino` 使用 `ArduinoJson`，請先安裝。
- 欄位命名採用 `w/sew`（white）對應 Sheet 格式；序列輸出標頭保留需求中的 `g/seg` 字樣。
