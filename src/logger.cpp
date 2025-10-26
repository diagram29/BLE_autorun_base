// logger.cpp

#include "logger.h" 
#include "FS.h"
// M5Stamp S3では、LittleFSの使用が推奨されますが、ここでは互換性のためSPIFFSを使用します。
// LittleFSを使う場合は、ボード設定でパーティションスキームを変更し、#include "LittleFS.h" に置き換えてください。
#include "SPIFFS.h" 

// ログファイル名
const char* LOG_FILE_PATH = "/operation_log.txt";

// =========================================================
// ログ機能の実装
// =========================================================

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