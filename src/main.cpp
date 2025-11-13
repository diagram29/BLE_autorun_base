#include <Arduino.h> // ⭐️ 修正: PlatformIOでSerial, Stringなどを使うために必須 ⭐️
#include <string>    // ⭐️ BLEのgetValue()を扱うために追加 ⭐️
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Wire.h>
#include "logger.h"  // ログ機能

#include <freertos/task.h> // FreeRTOS関連のヘッダ


// =========================================================
// 1. 定数 (CONSTANTS)
// =========================================================

// FreeRTOS キュー設定
#define MESSAGE_QUEUE_LENGTH 10
#define MESSAGE_MAX_SIZE 80 // 送信するメッセージの最大サイズ（バイト）


#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_TX "beb5484e-36e1-4688-b7f5-ea07361b26a8"

// リレーピン設定 (GPIO) と HIGH/LOW 定義
const int relay[]={8,10,6,7,5,4};  // GPIOピン リレー変数[0,1,2,3]  eps32[12,14,27,26] stamp pico[25,22,21,19]　上下左右
const int str[]={1,0}; // HIGH:1 LOW:0


#define INTERVAL 1000 //millisec


// =========================================================

//#include "BluetoothSerial.h" // ライブラリ
//#include "HardwareSerial.h"
//BluetoothSerial SerialBT; // SE通信のOJ宣言
//HardwareSerial Serial1(1); //UART1


// =========================================================
// 2. グローバル変数 (GLOBAL VARIABLES)
// =========================================================

// FreeRTOS
QueueHandle_t messageQueue;

// BLE
BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;

// 状態管理
String rxValue = "";  // 受信したBLEコマンド
String iptData;       // 処理中のコマンド
bool devCon = false, oldDevCon = false; // BLE接続状態
bool bReceived=false;             // 受信フラグ (使用箇所が少ないため整理可能だが現状維持)
bool emergency_stop_flag = false; // 緊急停止フラグ

// IO/動作設定
float downr,awe; // 下降設定値 (初期値), dows コマンド用変数
int val_ipt; //シリアル通信入力変数
unsigned long myTime = millis() / 1000; // 起動からの時間


// 自動往復(mit)用
int count = 0;
unsigned long lastCountTime = 0;

int interval = INTERVAL; //millisec


// 構造体の定義 (コードのグローバル変数定義付近に追加)
struct BleData {
    uint8_t category; // データの種類を示すカテゴリID (例: 1=走行時間, 2=残りのサイクル数)
    float value;      // 送りたい実際の数値 (例: 15.0秒, 10回)
};


// =========================================================
// 3. 関数プロトタイプ（フォワードデクラレーション）をここに追加
// =========================================================

// FreeRTOS タスク
void bleTask(void *pvParameters); // BLE処理タスク (Core 0で実行)
void ioTask(void *pvParameters);  // IO制御処理タスク (Core 1で実行)

// ユーティリティ/共通処理
void help();
void mes(String a);
float hen(String a , String b);
void mit(float ms);

// IO制御
void dows(String a,float b);
void emj();
void atlan();

// リレー制御 (rreyと上下左右・停止関数)
void rrey(String a,int b,int c,int d,int e);
void up(); void down(); void left(); void right();
void udstop(); void lrstop();
void nocostart(); void nocostop(); // ノコ制御

// リセット関数
void resetFunc(); // ★ 追加 ★




// =========================================================
// 4. BLE コールバッククラス (CALLBACKS)
// =========================================================

// BLEサーバー接続/切断コールバック
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {devCon = true;
                                      Serial.println("** 接続");};
  void onDisconnect(BLEServer* pServer) {
    devCon = false;
    //oldDevCon = false;
    Serial.println("** 切断");
    pServer->startAdvertising(); // アドバタイズリスタート
    Serial.println("アドバタイズスタート");
                                         }
};

// BLEキャラクタリスティック書き込みコールバック (受信処理)
class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    // 値を取得して String型のvalue変数に入れる)
    std::string stdRxValue = pCharacteristic->getValue();
        String tempRxValue = String(stdRxValue.c_str());
    if (tempRxValue.length() > 0) {
      tempRxValue.trim();
      Serial.println(tempRxValue);


    bReceived=true;

    // ⭐️ ログ機能の追加：コマンドを受信したらログに記録する ⭐️
    logData("RX_CMD: " + tempRxValue);

    int temp_val_ipt = tempRxValue.toInt();
    if (temp_val_ipt == 93 || temp_val_ipt == 99) {
        emergency_stop_flag = true; 
    }

    rxValue = tempRxValue;
}}};



// =========================================================
// 5. SETUP/LOOP
// =========================================================


//セットアップ処理
void setup()
{ //BT処理
  Serial.begin(921600);  //通信速度
  // ログ機能の初期化
   initLogger();
   logData("SYSTEM: Setup Start");
  // --- FreeRTOS初期化 --- メッセージキューの作成
    messageQueue = xQueueCreate(MESSAGE_QUEUE_LENGTH, MESSAGE_MAX_SIZE);
 
   //SerialBT.begin("ESP自動往復外用"); //SE通信開始 名称 1.自動往復1内用 2.自動往復2外用 3.STAMP開発用
  // Serial1.begin(19200,SERIAL_8N1,3,1);//IO3 RX  , IO1 TX

    // タスクの作成と起動 
    // IO処理タスクをCore 1で起動（優先度 5）
    xTaskCreatePinnedToCore(
        ioTask,      // 実行する関数名
        "IOTask",    // タスク名
        10000,       // スタックサイズ（バイト）
        NULL,        // パラメータ
        5,           // 優先度
        NULL,        // タスクハンドル
        1);          // Core 1 で実行

    // BLE処理タスクをCore 0で起動（優先度 4）
    xTaskCreatePinnedToCore(
        bleTask,     // 実行する関数名
        "BLETask",   // タスク名
        20000,       // スタックサイズ（バイト）
        NULL,        // パラメータ
        4,           // 優先度
        NULL,        // タスクハンドル
        0);          // Core 0 で実行

        // ⭐️ (2) タスクの起動これでもおｋ ⭐️
    //xTaskCreatePinnedToCore(ioTask, "IOTask", 10000, NULL, 5, NULL, 1);
    //xTaskCreatePinnedToCore(bleTask, "BLETask", 20000, NULL, 4, NULL, 0);

   //リレー初期処理 
    for(int i=0;i<6;i++){pinMode(relay[i], OUTPUT); 
                         digitalWrite(relay[i], str[1]);} //リレー1-4　選択 オフ
    downr = 0.5;

    logData("SYSTEM: Setup Finished");
    readAndPrintLog();//起動時にログ内容をシリアルに出力(テスト)
}
void loop() {}//もういらない


// =========================================================
// 6. FREE RTOS タスク定義 (TASK DEFINITIONS)
// =========================================================

// --- Core 0: BLE処理タスク ---
void bleTask(void *pvParameters) {
    
  // BLEデバイスの初期化
     BLEDevice::init("テスト用M5"); // デバイス名を設定
    //サーバーを作成
     pServer = BLEDevice::createServer();
     pServer->setCallbacks(new MyServerCallbacks());
    // サービスを作成
     BLEService *pService = pServer->createService(SERVICE_UUID);
    // キャラクタリスティックを作成
     pTxCharacteristic = pService->createCharacteristic(
                    CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);                   
     pTxCharacteristic->addDescriptor(new BLE2902());

     BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
                    CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
    // RXキャラクタリスティックに初期値を設定
     pRxCharacteristic->setCallbacks(new MyCallbacks());
     pService->start();// サービスを開始
     pServer->getAdvertising()->start();// BLE広告を開始

   char receivedMsg[MESSAGE_MAX_SIZE];


    // タスクのメインループ
    for (;;) {
        // キューからメッセージを受信（ブロック待機時間：10ミリ秒）
        if (xQueueReceive(messageQueue, receivedMsg, pdMS_TO_TICKS(10)) == pdPASS) {
            // メッセージを受信したら、BLEで通知を送信
            if (devCon && pTxCharacteristic != NULL) {
                pTxCharacteristic->setValue(receivedMsg);
                pTxCharacteristic->notify();
            }
        }
        
        // 接続状態のチェックとアドバタイズ再開処理
        if (!devCon && oldDevCon) {
            oldDevCon = devCon;
        }
        if (devCon && !oldDevCon) {
            oldDevCon = devCon;
        }

        // 💡 重要な修正: loop()内の delay(2) の代わりに vTaskDelay を使用 
        // 他のタスクにCPU使用権を譲る
        vTaskDelay(pdMS_TO_TICKS(5)); 
    }
}



// --- Core 1: IO制御タスク ---メイン処理
void ioTask(void *pvParameters) {
    
  // 起動時メッセージをここで送信
    mes("\nようこそ！自動走行システムへ"
        "\n下降設定値は現在0.5秒"
        "\n上下左右の各処理はそれぞれのボタンを押してください");


  // タスクのメインループ
    for (;;) {
        //ArduinoOTA.handle();
        // BTデータ取得 (接続チェックはbleTaskが担当するのでシンプルに)
        if (devCon) {
            if (rxValue.length() > 0) {
               iptData = rxValue;
               val_ipt = iptData.toInt();// コマンド処理開始前にリセット

               myTime = millis()/1000;
               mes("処理" + String(iptData) + "を受付\n起動から" + String(myTime) + "秒経過中…");
               atlan();
           /*区切りから区切りまでのデータを取得
            iｆ分そのものをオブジェクト化するとやりやすいかも
            データ入りがあった場合インプットデータにシリアルから来たデータを一文字ずつ読み取り区切りまでのデータを入れる
            そのデータをif分をもとに場合分けしてスイッチをかまして各動作に反映しているほかに自動処理に分けているそれ以外の文字をブロックしてスレーブに返している
            
            複数回の；まで受信して順番に処理していくように記述する回数分ループ処理する
            スレーブ側のデータをマスタが受け取ったときに；区切りで順番に処理するように記述したい
           */

// --- コマンド解析と実行 ---

     if(iptData == "help"){help();}
        //下降タイマー
else if(iptData.startsWith("dows")){ awe=hen("s","x");dows(String(awe)+"秒降下",awe);}
        //下降値設定
else if(iptData.startsWith("setd")){ downr=hen("d","x");mes("下降値を" + String(downr) + "に設定しました");}
        //ログ表示
else if(iptData == "showlog"){ 
        readAndPrintLog(); mes("ログをシリアルに出力しました"); 
        sendLogChunk(pTxCharacteristic); // 👈 mes()を呼び出していないので安全
}
else if(iptData == "restart"){ 
        mes("再起動コマンドを受付ました。システムを再起動します。");
        resetFunc(); // リセットを実行
}

//リレー入力処理
else if (val_ipt == 93) {emj();}   //緊急停止
else{ switch (val_ipt) {

  //リレー駆動
    case 11:up();      break;  //上昇
    case 10:udstop();  break;  //上下停止
    case 12:down();    break;  //下降
    case 21:left();    break;  //左走行
    case 20:lrstop();  break;  //走行停止
    case 22:right();   break;  //右走行  
    case 31:nocostart();break;  //ノコスタート
    case 30:nocostop(); break;  //ノコ停止
  
    case 99:emj();break; //現在の動作が終了次停止
    default:mes("入力は半角英数字です範囲外または無効です\nコマンドの連続送信に注意"); break;}
}
        rxValue = "";
            }
        }

        // 💡 重要な修正: 処理がないときの vTaskDelay 
        // 他のタスクにCPU使用権を譲る
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}



// =========================================================
// 7. ユーティリティ/共通関数 (UTILITY FUNCTIONS)
// =========================================================


void help(){mes("\n-- 自動走行システム ヘルプ --"
                "\n下降設定値は現在0.5秒です。"
                "\n-- オートパイロット (atlan) --\n"
          
                "\n起動は以下のように記述してください。"
                "\n 例: atl15s10c" 
                "\n(左から右へ片道15秒、往復10サイクル)"
          
                "\n頭文字atでモード判断、L/Rで始動位置を指定します。"
                "\n秒数(s)で移動距離、サイクル数(c)で繰り返しを認識します。\n"
            
                "\n-- その他コマンド --"
                "\n入力はすべて半角英数字です。"
                "\nsetdx: 下降値を変更 (例: setd0.3x)"
                "\ndowsx: 設定秒数降下 (例: dows0.8x)"
                
                "\n-11:up        上昇"
                "\n-10:udstop    上下停止"
                "\n-12:down      下降\n"

                "\n-21:left      左走行"
                "\n-20:lrstop    走行停止"
                "\n-22:right     右走行\n"

                "\n-31:nocostart ノコスタート"
                "\n-30:nocostop  ノコ停止\n"

                "\n-99:emj       現在の動作が終了次停止");
                                }


// メッセージ送信 (キューに投入)
void mes(String a){//SerialBT.println(a);
      String message_with_newline = a + "\n";
      Serial.println(a);
        logData("TX_MES: " + a);

        // ⭐️ 修正: キューにメッセージをコピーして送信 ⭐️
    if (messageQueue != NULL) {
        // メッセージをC文字列に変換し、キューに送信
        char msgBuffer[MESSAGE_MAX_SIZE];
        message_with_newline.toCharArray(msgBuffer, MESSAGE_MAX_SIZE);
        
        // メッセージをキューに送信（待機時間：0）
        xQueueSend(messageQueue, msgBuffer, 0);
    }
}



//入ったデータの始点が""の文字の時　変数に"起点"に一つ足した文字位置から後方の文字を文字として入れる
float hen(String a , String b) {float result = String(iptData.substring(int(iptData.indexOf(a))+1,int(iptData.indexOf(b)))).toFloat();
                                return result ;}

//delay処理による完全停止を回避するための構文
void mit(float ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {

    // ----------------------------------------------------------------
    // 【💡 10秒ごとのカウントアップ処理】
    // ----------------------------------------------------------------
    unsigned long correntTime = millis();
    
    // 前回カウント処理をしてから10秒（counthterval）以上経過したかチェック
    if(correntTime - lastCountTime >= 10000){

        // 経過時刻を更新（次の10秒を測るため）
        lastCountTime = correntTime;
        
        // カウントを増やす
        count = count + 10;
        // メッセージをBLEとシリアルに出力
        // mes関数はBLE通知も行います
        mes(String(count) + "秒,経過しました ");//後ほど速度も追加する
       
    };
     




  // 💡 FreeRTOS環境ならここに vTaskDelay(1); を入れるのが理想
   vTaskDelay(pdMS_TO_TICKS(1));

  //if(SerialBT.available()){break;}
  }
// 待機終了後、合計カウント値をリセット（デモのため）
  count = 0;
  // 次の mit 呼び出しに備えて時刻をリセット
  lastCountTime = millis();

}


void sendData(uint8_t category_id, float numerical_value) {
    // 1. データ構造体に値を格納
    BleData data;
    data.category = category_id;
    data.value = numerical_value;

    // 2. シリアルに出力（デバッグ用）
    Serial.printf("TX_DATA: Category=%d, Value=%.2f\n", category_id, numerical_value);
    
    // 3. ログに記録
    logData("TX_DATA: Cat:" + String(category_id) + ", Val:" + String(numerical_value, 2));

    // 4. キューに構造体をコピーして送信
    if (messageQueue != NULL) {
        // 構造体をキューに送信（待機時間：0）
        xQueueSend(messageQueue, &data, 0);
    }
}

void resetFunc() {
    //mes("\n*** SYSTEM: Performing software restart (ESP.restart()) ***");
    Serial.flush(); // シリアル出力が完了するのを待つ
    // mes()がキューに届き、bleTaskで処理されるのを少し待つ
    vTaskDelay(pdMS_TO_TICKS(100)); 
    ESP.restart(); }






// =========================================================
// 8. IO制御とロジック関数 (IO CONTROL & LOGIC)
// =========================================================


//各リレーへ信号送信処理
void rrey(String a,int b,int c,int d,int e){mes(a); 
  for (int i = b; i < c+1; i++) {pinMode(relay[i], OUTPUT);}
        digitalWrite(relay[b], str[d]);
        digitalWrite(relay[c], str[e]);}     // HIGH:1 LOW:0
    void up     (){rrey("上昇します",      0,1,      0,1);}
    void down   (){rrey("下降します",      0,1,      1,0);}

    void left   (){rrey("奥へ走行します",    2,3,      0,1);}
    void right  (){rrey("手前へ走行します",    2,3,      1,0);}

    void udstop (){rrey("上下を停止します",  0,1,      1,1);}
    void lrstop (){rrey("走行を停止します",  2,3,      1,1);}

    void nocostart (){rrey("ノコを回転します", 4,4,  1,1);}
    void nocostop  (){rrey("ノコを停止",     4,4,  0,0);}

void emj(){mes("緊急停止""\n現在" + String(myTime) + "秒経過");  //緊急停止 信号
   for(int i=0;i<6;i++){pinMode(relay[i], OUTPUT); 
                        digitalWrite(relay[i], str[1]);}} //全リレー　選択 オフ

//降下処理
void dows(String a,float b){mes(a); down(); mit(b*1000); udstop();}

//自動往復処理
void atlan(){
  if(iptData.startsWith("at")){ int as1 = 0,sc2,cb3,bd4,start1 = 0;
            if(iptData.startsWith("atl")){as1=hen("l","s");start1 =1;} /*始動位置が左からの場合*/
       else if(iptData.startsWith("atr")){as1=hen("r","s");start1 =2;} /*始動位置が右からの場合*/
         sc2=hen("s","c");cb3=hen("c","b");bd4=hen("b","d");

        for(int i =0;i<sc2;i++){
          int i2 =i+1;
          int kkt =as1*2*(sc2-i2); 
            for(int vi =0;vi<2;vi++){ 
              if (emergency_stop_flag) {
                    emj(); // 緊急停止処理を実行（これはemj()でリレーも停止するからOK）
                    emergency_stop_flag = false; // フラグを戻す
                    goto bailout; // ループから脱出
                }
             
             /*繰り返し処理メイン*/
             switch(start1){case 1: left();  mes("奥へ" + String(as1) + "秒移動中です"); break; 
                            case 2: right(); mes("手前へ" + String(as1) + "秒移動中です"); break;} /*左から右へ  と 右から左へ　のスタート折り返しと逆転*/
                  
               mit(as1*1000);

             switch(start1){case 1: mes("修正" + String(cb3) + "秒/10 移動"); mit(cb3*100); start1 =2; break;
                            case 2: mes("修正" + String(bd4) + "秒/10 移動"); mit(bd4*100); start1 =1; break; } 
               lrstop(); mit(200); dows(String(downr) +"秒降下",downr); mit(200);
               mes(" \n折返します");}
               
            mes("\n現在片道" + String(as1) + "秒往復の" + String(i2) + "/" + String(sc2) + "回目です"
                        "\nあと" + String(kkt) + "秒です"
                        //"\n片下げ0.5cmの場合" + String(i2) + "/" + String(sc2*2) + "cmです"
                        "\n速度を変えると距離が変化しますご注意下さい"
                        "\nコマンド" + String(iptData) + "実行中…\n");
 }
mes("\n指示" + String(iptData) + "処理が終了しました"
        "\nシステム起動から" + String(myTime) + "秒経過中…");
}bailout:;}
        

