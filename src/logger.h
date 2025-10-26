// logger.h

#ifndef LOGGER_H
#define LOGGER_H

// <Arduino.h>はmain.cppでインクルードされているが、
// C++標準から外れるString型などを使うため、念のため含める
#include <Arduino.h> 

// ログ機能の関数プロトタイプ宣言
void initLogger();
void logData(const String& logMessage);
void readAndPrintLog();

// 将来的にBLE経由でのログ転送用関数もここに追加可能
// void getLogChunk(int chunkIndex, char* buffer, size_t bufferSize);

#endif // LOGGER_H