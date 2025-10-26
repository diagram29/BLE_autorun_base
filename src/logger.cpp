// logger.cpp

#include "logger.h" 
#include "FS.h"
// M5Stamp S3では、LittleFSの使用が推奨されますが、ここでは互換性のためSPIFFSを使用します。
// LittleFSを使う場合は、ボード設定でパーティションスキームを変更し、#include "LittleFS.h" に置き換えてください。
#include "SPIFFS.h" 

// ログファイル名
const char* LOG_FILE_PATH = "/operation_log.txt";
const size_t CHUNK_SIZE = 500;
// =========================================================
// ログ機能の実装（抜粋と修正）
// =========================================================

// initLogger() と logData() は変更なし。（logDataはSPIFFSへの書き込みのみ）

/**
 * @brief ログファイルを読み出し、BLECharacteristic経由でチャンク送信します。
 * @param pTxCharacteristic BLEの送信特性ポインタ
 */
/**
 * @brief SPIFFSファイルシステムを初期化します。
 */
void initLogger() {
  // true: マウント失敗時に自動的にフォーマットを試みる
  if (!SPIFFS.begin(true)) { 
    Serial.println("SPIFFS マウントに失敗しました。ログ保存はできません。");
    return;
  }
  Serial.println("SPIFFS マウント成功。");
  
  // 開発中、毎回ログを消したい場合は以下のコメントを外す
  // if (SPIFFS.exists(LOG_FILE_PATH)) {
  //   SPIFFS.remove(LOG_FILE_PATH);
  //   Serial.println("既存のログファイルを削除しました。");
  // }
}

/**
 * @brief ログデータをファイルに追記します。
 * @param logMessage 記録したいメッセージ
 */
void logData(const String& logMessage) {
  // ファイルを追記モード (FILE_APPEND) で開く
  File logFile = SPIFFS.open(LOG_FILE_PATH, FILE_APPEND);
  
  if (!logFile) {
    Serial.println("WARN: ログファイルを開けませんでした (追記エラー)");
    return;
  }

  // タイムスタンプの作成（ミリ秒 / 1000 で秒数）
  unsigned long currentTime = millis() / 1000;
  String logEntry = String("[T:") + currentTime + "s] " + logMessage + "\n";
  
  // ファイルに書き込み
  if (logFile.print(logEntry)) {
    // 成功。シリアルモニタへの出力は負荷軽減のため省略しても良い
    // Serial.print("LOGGED: ");
    // Serial.print(logEntry);
  } else {
    Serial.println("WARN: ログ書き込みエラー");
  }
  
  // ファイルを閉じる
  logFile.close();
}

/**
 * @brief ログファイルを読み出し、BLECharacteristic経由でチャンク送信します。
 * @param pTxCharacteristic BLEの送信特性ポインタ
 */
void sendLogChunk(BLECharacteristic* pTxCharacteristic) {
    if (!pTxCharacteristic) {
        Serial.println("ERROR: BLE特性ポインタが無効です。ログを送信できません。");
        return;
    }

    File logFile = SPIFFS.open(LOG_FILE_PATH, FILE_READ);
    
    if (!logFile) {
        Serial.println("ERROR: ログファイルが見つかりません。");
        // 受信側にエラーを通知するために、BLEでメッセージを送る
        pTxCharacteristic->setValue("ERROR: Log file not found.");
        pTxCharacteristic->notify();
        return;
    }

    // 転送開始メッセージをBLEで送信
    String startMsg = "--- LOG START (Total Size: " + String(logFile.size()) + " bytes) ---\n";
    pTxCharacteristic->setValue(startMsg.c_str());
    pTxCharacteristic->notify();
    Serial.print(startMsg); // シリアルモニタにも出力
    delay(50); 
    
    char buffer[CHUNK_SIZE + 1]; 

    while (logFile.available()) {
        size_t bytesRead = logFile.readBytes(buffer, CHUNK_SIZE);
        buffer[bytesRead] = '\0'; 

        // 読み出したデータをBLEで送信
        pTxCharacteristic->setValue(buffer);
        pTxCharacteristic->notify();
        
        // 転送間隔。受信側の処理能力に応じて調整
        delay(50); 
    }

    logFile.close();

    // 転送完了メッセージをBLEで送信
    pTxCharacteristic->setValue("--- LOG COMPLETE ---");
    pTxCharacteristic->notify();
    Serial.println("--- LOG COMPLETE ---");
}






/**
 * @brief 保存されている全ログをシリアルモニタに出力します。
 */
void readAndPrintLog() {
  Serial.println("\n--- SPIFFS LOG FILE CONTENT START ---");
  File logFile = SPIFFS.open(LOG_FILE_PATH, FILE_READ);
  
  if (!logFile) {
    Serial.println("ERROR: ログファイルが見つかりません。");
    return;
  }
  
  // ファイルの内容をすべてシリアルに出力
  while(logFile.available()){
    Serial.write(logFile.read());


  }
  
  Serial.println("--- SPIFFS LOG FILE CONTENT END ---");
  logFile.close();
}