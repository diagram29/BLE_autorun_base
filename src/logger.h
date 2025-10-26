// logger.h

#ifndef LOGGER_H
#define LOGGER_H

// <Arduino.h>はmain.cppでインクルードされているが、
// C++標準から外れるString型などを使うため、念のため含める
#include <Arduino.h>
#include <BLEServer.h> // BLECharacteristicを使うために必要 

// ログ機能の関数プロトタイプ宣言
void initLogger();
void logData(const String& logMessage);
void readAndPrintLog();

// ⭐️ 新規：ログをチャンク単位でBLE送信する関数 ⭐️
void sendLogChunk(BLECharacteristic* pTxCharacteristic);

#endif // LOGGER_H