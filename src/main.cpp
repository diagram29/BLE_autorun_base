#include <Arduino.h> // ⭐️ 修正: PlatformIOでSerial, Stringなどを使うために必須 ⭐️
#include <string>    // ⭐️ BLEのgetValue()を扱うために追加 ⭐️
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Wire.h>
#include "logger.h"

#define INTERVAL 1000 //millisec

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_TX "beb5484e-36e1-4688-b7f5-ea07361b26a8"

// =========================================================
// 3. 関数プロトタイプ（フォワードデクラレーション）をここに追加
// =========================================================
void help();
void mes(String a);
float hen(String a , String b);
void mit(float ms);
void dows(String a,float b);
void emj();

// rrey と関連する上下左右・停止関数
void rrey(String a,int b,int c,int d,int e);
void up(); void down(); void left(); void right();
void udstop(); void lrstop();

void atlan();
// =========================================================

//#include "BluetoothSerial.h" // ライブラリ
//#include "HardwareSerial.h"
//BluetoothSerial SerialBT; // SE通信のOJ宣言

//HardwareSerial Serial1(1); //UART1

BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;

String rxValue = "";
bool devCon = false, oldDevCon = false;
bool bReceived=false;
int interval = INTERVAL; //millisec

bool emergency_stop_flag = false;

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

class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    // 値を取得して String型のvalue変数に入れる)
    std::string stdRxValue = pCharacteristic->getValue();
        String tempRxValue = String(stdRxValue.c_str());
    if (tempRxValue.length() > 0) {
      tempRxValue.trim();
      Serial.println(tempRxValue);
      pTxCharacteristic->setValue(tempRxValue.c_str());
      pTxCharacteristic->notify();
      delay(10);
    bReceived=true;

    // ⭐️ ログ機能の追加：コマンドを受信したらログに記録する ⭐️
    logData("RX_CMD: " + tempRxValue);

    int temp_val_ipt = tempRxValue.toInt();
    if (temp_val_ipt == 93 || temp_val_ipt == 99) {
        emergency_stop_flag = true; 
    }

    rxValue = tempRxValue;
}}};

unsigned long myTime = millis() / 1000;
const int relay[]={8,10,6,7,5,4};  // GPIOピン リレー変数[0,1,2,3]  eps32[12,14,27,26] stamp pico[25,22,21,19]　上下左右
const int str[]={1,0}; // HIGH:1 LOW:0
int val_ipt; //シリアル通信入力変数

float downr,awe;
String iptData;


//セットアップ処理
void setup()
{ //BT処理
  Serial.begin(921600);  //通信速度
  //SerialBT.begin("ESP自動往復外用"); //SE通信開始 名称 1.自動往復1内用 2.自動往復2外用 3.STAMP開発用

 // Serial1.begin(19200,SERIAL_8N1,3,1);//IO3 RX  , IO1 TX

 // ⭐️ ログ機能の初期化を追加 ⭐️
   initLogger();
    logData("SYSTEM: Setup Start");


 // BLEデバイスの初期化
  BLEDevice::init("autobredeRAN2"); // デバイス名を設定
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






  mes("\nようこそ！自動走行システムへ"
      "\n下降設定値は現在0.2秒"
      "\n上下左右の各処理はそれぞれのボタンを押してください");
   //リレー初期処理 
    for(int i=0;i<6;i++){pinMode(relay[i], OUTPUT); 
                         digitalWrite(relay[i], str[1]);} //リレー1-4　選択 オフ
    downr = 0.5;

    logData("SYSTEM: Setup Finished");

    // ⭐️ テスト用：起動時にログ内容をシリアルに出力 ⭐️
   readAndPrintLog();
}

void help(){mes("\n-- 自動走行システム ヘルプ --");
            mes("下降設定値は現在0.2秒です。");
            mes("\n-- オートパイロット (atlan) --");
          
            mes("起動は以下のように記述してください。");
            mes(" 例: atl15s10c"); 
            mes(" (左から右へ片道15秒、往復10サイクル)");
          
            mes("\n頭文字atでモード判断、L/Rで始動位置を指定します。");
            mes("秒数(s)で移動距離、サイクル数(c)で繰り返しを認識します。");
            
            mes("\n-- その他コマンド --");
            mes(" setdx: 下降値を変更 (例: setd0.3x)");
            mes(" dowsx: 設定秒数降下 (例: dows0.8x)");
            mes(" 入力はすべて半角英数字です。");
            mes("\n-11:up        上昇"
                "\n-10:udstop    上下停止"
                "\n-12:down      下降\n"

                "\n-21:left      左走行"
                "\n-20:lrstop    走行停止"
                "\n-22:right     右走行\n"

                "\n-31:nocostart ノコスタート"
                "\n-30:nocostop  ノコ停止\n"

                "\n-99:emj       現在の動作が終了次停止");
                                }
//メッセージを表示する
void mes(String a){//SerialBT.println(a);
      String message_with_newline = a + "\n";
      Serial.println(a);
        logData("TX_MES: " + a);
        // ⭐️ 修正: StringをC文字列(char*)に変換 ⭐️
        pTxCharacteristic->setValue(message_with_newline.c_str());
        pTxCharacteristic->notify();
        delay(10);

}




//入ったデータの始点が""の文字の時　変数に"起点"に一つ足した文字位置から後方の文字を文字として入れる
float hen(String a , String b) {float result = String(iptData.substring(int(iptData.indexOf(a))+1,int(iptData.indexOf(b)))).toFloat();
                                return result ;}

//delay処理による完全停止を回避するための構文
void mit(float ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
  //if(SerialBT.available()){break;}
  }}
//降下処理
void dows(String a,float b){mes(a); down(); mit(b*1000); udstop();}


void emj(){mes("緊急停止""\n現在" + String(myTime) + "秒経過");  //緊急停止 信号
   for(int i=0;i<6;i++){pinMode(relay[i], OUTPUT); 
                        digitalWrite(relay[i], str[1]);}} //リレー1-4　選択 オフ

        
//各リレーへ信号送信処理
void rrey(String a,int b,int c,int d,int e){mes(a); 
  for (int i = b; i < c+1; i++) {pinMode(relay[i], OUTPUT);}
        digitalWrite(relay[b], str[d]);
        digitalWrite(relay[c], str[e]);}     // HIGH:1 LOW:0
    void up     (){rrey("上昇します",      0,1,      0,1);}
    void down   (){rrey("下降します",      0,1,      1,0);}

    void left   (){rrey("右走行です",    2,3,      0,1);}
    void right  (){rrey("左走行です",    2,3,      1,0);}

    void udstop (){rrey("上下を停止します",  0,1,      1,1);}
    void lrstop (){rrey("走行を停止します",  2,3,      1,1);}

    void nocostart (){rrey("ノコを回転します", 4,4,  1,1);}
    void nocostop  (){rrey("ノコを停止",     4,4,  0,0);}


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
             switch(start1){case 1: left();  break; 
                            case 2: right(); break;} /*左から右へ  と 右から左へ　のスタート折り返しと逆転*/

               mes("片道" + String(as1) + "秒移動中です"); mit(as1*1000);

             switch(start1){case 1: mes("修正" + String(cb3) + "秒/10 移動"); mit(cb3*100); start1 =2; break;
                            case 2: mes("修正" + String(bd4) + "秒/10 移動"); mit(bd4*100); start1 =1; break; } 
               lrstop(); mit(200); dows(String(downr) +"秒降下",downr); mit(200);
               mes(" \n折返します");}
               
            mes("\n片道" + String(as1) + "秒往復の" + String(i2) + "/" + String(sc2) + "回目です"
                        "\nあと" + String(kkt) + "秒です"
                        "\n片下げ0.5cmの場合" + String(i2) + "/" + String(sc2*2) + "cmです"
                        "\n速度を変えると距離が変化しますご注意下さい"
                        "\nコマンド" + String(iptData) + "実行中…\n");
 }
mes("\n指示" + String(iptData) + "処理が終了しました"
        "\nシステム起動から" + String(myTime) + "秒経過中…");
}bailout:;}


//BLE用プログラム

//メイン処理
void loop() { 
 //ArduinoOTA.handle();
  //BTデータ取得
  if (devCon) {
  if (rxValue.length() > 0) {
    iptData = rxValue;
    val_ipt = iptData.toInt();
    myTime = millis()/1000;
    mes("処理" + String(iptData) + "を受付\n起動から" + String(myTime) + "秒経過中…");
    atlan();
    //区切りから区切りまでのデータを取得
    //iｆ分そのものをオブジェクト化するとやりやすいかも
    //データ入りがあった場合インプットデータにシリアルから来たデータを一文字ずつ読み取り区切りまでのデータを入れる
    //そのデータをif分をもとに場合分けしてスイッチをかまして各動作に反映しているほかに自動処理に分けているそれ以外の文字をブロックしてスレーブに返している

    //複数回の；まで受信して順番に処理していくように記述する回数分ループ処理する
    //スレーブ側のデータをマスタが受け取ったときに；区切りで順番に処理するように記述したい

    if(iptData == "help"){help();}
    //下降タイマー
else if(iptData.startsWith("dows")){ awe=hen("s","x");dows(String(awe)+"秒降下",awe);}
    //下降値設定
else if(iptData.startsWith("setd")){ downr=hen("d","x");mes("下降値を" + String(downr) + "に設定しました");}
// ⭐️ 新規コマンド: ログ表示 ⭐️
else if(iptData == "showlog"){ 
  readAndPrintLog(); mes("ログをシリアルに出力しました"); 
  sendLogChunk(pTxCharacteristic); // 👈 mes()を呼び出していないので安全
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
}else{bReceived=false;}
  // 切断
  if (!devCon && oldDevCon) {
    oldDevCon = devCon;
  }
  // 接続
  if (devCon && !oldDevCon) {oldDevCon = devCon;}
  delay(2);
}
            