/*
*ヘッダファイル: "SketchWriter"
*  最終更新日: 6.12.2016
*
*  説明:
*    optiboot―Arduino UNOに書き込まれているブートローダーを操作します.
*    SDカードにあるHexFileを読み込み, optibootにそのデータを送信することでスケッチを書き込むことができます; パソコンを用いずに.
*
*  必須ヘッダファイル:
*    このヘッダファイルをインクルードする前に以下のライブラリをインクルードしてください.
*      SD.h
*      SPI.h
*
*  対応しているArduino:
*    optiboot操作用Arduino; このヘッダファイルをインクルードするArduino:
*      動作周波数:
*        16MHz
*
*    操作されるArduino:
*      optibootが書きこまれているArduino
*        Arduino UNO
*
*  このヘッダを使用する前の準備:
*    optibootを操作するArduino_Aと書き込まれるArduino_Bを用意する.
*    Arduino_AとSDカードを接続する.
*      このときCSピンは任意の場所にさしてよい.
*
*  使用例:
*    スケッチ"Blink"をコンパイルした後に作成されたHexFile"Blink.hex"を使ってスケッチ"Blink"を書き込む.
*

#include <SD.h>
#include <SPI.h>
#include "SketchWriter.h"

//インスタンス生成
SketchWriter sketchWriter;

void setup() {
//SketchWriterを開始します*7_1
sketchWriter.Begin(10);
sketchWriter.SetResetPin(6);

//HexFileをロードします*7_2
sketchWriter.SketchLoad("Blink.hex");
}

void loop() {
//10秒待機
delay(10000);

//Arduino_Bにリセットをかけます; optibootが起動します.
sketchWriter.ResetArduino(6);

//optibootが起動しているか確認します.*7_3
sketchWriter.GetSync();

//読み込んだHexFileのデータをoptibootに送信します; スケッチが書き込まれます.
sketchWriter.SketchWrite();

//optibootにプログラムを実行するよう命令します.
sketchWriter.AppStart();

//開いていたHexFileを閉じます.*7_4
sketchWriter.SketchClose();

while(1);
}

*      説明:
*        *7_1:
*          ここでは,SDカードのチップセレクトピンを10番ピンに設定,
*          Arduino_Bにリセット信号を送信するピンを6番ピンに設定しています.
*          Arduino_Bのリセットピンとこのピンをつなぐことになります.
*
*        *7_2:
*          ファイル名は6文字+"."+3文字である必要があります;SDライブラリの仕様上.
*          詳しくは"Arduino日本語リファレンス"を参照してください.
*
*        *7_3:
*          Arduino_Bにリセットをかけた後に一回実行するとよいでしょう.
*          optibootが起動していない―命令を受け付けていない―ときにHexFileのデータを送ることはできません.
*
*        *7_4:
*          開いていたHexFileを閉じたあと別のHexFileを開くことができます.
*
*  更新履歴:
*    2.1.2016:
*      プログラムの完成
*
*    2.2.2016:
*      説明欄更新
*
*    2.3.2016:
*      コマンド"readHexData", "sendData"を訂正.
*        HexFile各レコードごとのデータ読み込みをやめ,指定するかℨのデータ読み込むようにした.
*        レコードごとHexデータを送信することをやめ,指定する数のデータを送信するようにした.
*        というのも,HexFileの終わりを除くすべてのレコードが16個のデータを持つとは限らないため.
*
*      コマンド"putToHexDataArray"は改良の余地あり.
*        このコマンドはコマンド"readData"の機能を含んでいる.
*        各コマンドは,ほかのコマンドの機能にかぶらないように設計されるべきである.
*
*    2.4.2016:
*      コマンド"putToHexDataArray"を廃止
*      コマンド"readHexVal"を追加
*      コマンド"sketchReload"を追加
*      コマンド"strlenS"を追加
*      スケッチを再送信できない問題を修正
*      一部メモリの解放をしていない問題を修正
*
*   4.22.2016:
*       スクリプト修正
*
*  6.12.2016:
*    マクロ定義を減らした; 今後ほかのマクロ定義と重複するのを防ぐため
*
*  詳しい情報:
*    詳しい情報をほしい方は以下のサイトを参考にしてください.
*      http://kanrec.web.fc2.com/contents/informatics/contents/Arduino/contents/SketchWriter/contents/usage_SketchWriter/index_usage_SketchWriter.html
*        注:リンクが切れているときはスミマセン
*/



#ifndef _SKETCH_WRITER_
#define _SKETCH_WRITER_

#include <stdlib.h>
#include "Arduino.h"
#include "stk500.h"

//
//デバックモード
//  説明:
//    下のコードを有効にしてArduinoに書き込むとデバックモードで動作します.
//    シリアル通信速度を15200bpsに設定してシリアルモニターを確認してください.
//    現在どこの関数にいるか, 何を送信しているか, どこの関数で無限ループしているかを確認できます.
//    デバックモードのときはoptibootを操作できません.
//    デバック時は操作されるArduinoと接続を切るとよいでしょう.
//#define _DEBUG_ALL_

#ifdef _DEBUG_ALL_
#define _DEBUG_ReadHexData_
#define _DEBUG_SetAddress_
#define _DEBUG_SendData_
#define _DEBUG_GetParameter_
#define _DEBUG_SketchWrite_
#define _DEBUG_GetSync_
#define _DEBUG_AppStart_
#define _DEBUG_SketchLoad_
#endif

#define OPTIBOOT_MAJVER 4
#define OPTIBOOT_MINVER 4

//#define TRY_MAX 1000


class SketchWriter
{
  private:

    const int  serialSpeed = 115200;
    const int  sendDataSize = 128;
    const int  hexDataBufSize = 16;

    //HEXファイル1レコード分のデータ配列
    byte *hexData;

    //1レコードで未送信のデータの数
    byte leftOver;

    //HexFileの最後まで来ているかの変数
    //true: ファイルの最後に到達した
    //false: ファイルの途中である
    boolean fileEnded;

    char *sketchName;

    //メモリ位置共用体
    typedef union
    {
      word val;
      struct
      {
        byte lo;
        byte hi;
      } lohi;
    } ADDRESS_POS;

    //SDファイル構造体
    File fp;

  public:
    //===コード==================================================================================

    //---
    //
    //SketchWriter開始, 終了関数セット
    //
    //---

    //関数: void Begin(byte pinCS)
    //  説明:
    //    SketchWriterを初期化します.
    //  引数:
    //    byte pin_cs:
    //      SDカードのチップセレクトピン
    //
    //    byte pin_rst:
    //      Arduinoのリセットピン
    //
    void Begin(byte pinCS)
    {

      //シリアル通信開始
      Serial.begin(serialSpeed);

      //SDカード設定
      pinMode(pinCS, OUTPUT);
      SD.begin(pinCS);
      
      //hexData格納配列メモリ確保
      hexData = new byte[hexDataBufSize];
    }

    void SetResetPin(byte pin)
    {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, HIGH);
    }

    //
    //関数: boolean End(void)
    //  説明: SketchWriterを終了します.
    //
    boolean End(void)
    {
      delete(hexData);
      
      Serial.end();
      fp.close();
      return true;
    }



    //---
    //
    //optiboot通信用関数セット
    //
    //---

    //
    //関数: void ResetArduino(byte pin)
    //  説明:
    //    Arduinoをリセットします.
    //
    void ResetArduino(byte pin)
    {
      byte n;

      for (n = 0; n < 3; n++)
      {
        digitalWrite(pin, LOW);
        digitalWrite(pin, HIGH);

        delay(500);
      }
    }

    //
    //関数: byte GetCh(void)
    //  説明: optibootから送られてくるシリアルデータを受信します.
    //        送られてくるまで待機します.
    //
    byte GetCh(void)
    {
      int ch;

      for (;;)
      {
        ch = Serial.read();
        if (ch != -1)
        {
          break;
        }
      }

      return (byte)ch;
    }


    //
    //関数: void Wait(void)
    //  説明:
    //    optibootからの処理完了信号(STK_OK)が来るまで待機します.
    //
    void Wait(void)
    {
      while (GetCh() != STK_OK);
    }


    //
    //関数: void GetInSync(void)
    //  説明:
    //    optibootからのSTK_INSYNC信号が来るまで待機します.
    //
    void GetInSync(void)
    {
      while (GetCh() != STK_INSYNC);
    }

    //
    //関数: void VerifySpace(void)
    //  説明:
    //    送信したコマンドを有効にします.
    //
    void VerifySpace(void)
    {
      Serial.write(CRC_EOP);
      GetInSync();
    }


    //
    //関数: void AppStart(void)
    //  説明:
    //    optibootにプログラムを実行するよう命じます.
    //
    void AppStart(void)
    {
#ifdef _DEBUG_AppStart_
      Serial.print(F("\r\n"));
      Serial.println(F("[AppStart] Start"));
#endif
      SerialClear();

      Serial.write(STK_LEAVE_PROGMODE);

#ifndef _DEBUG_AppStart_
      VerifySpace();
#endif

#ifdef _DEBUG_AppStart_
      Serial.print(F("\r\n"));
      Serial.println(F("[AppStart] End"));
#endif
    }

    //
    //関数: GetSync(void)
    //  説明:
    //    optibootと同期します.
    //    optiboot起動時に実行するとよいでしょう.
    //
    void GetSync(void)
    {

#ifdef _DEBUG_GetSync_
      Serial.print(F("\r\n"));
      Serial.println(F("[GetSync] Start"));
#endif

#ifndef _DEBUG_GetSync_
      for (;;)
      {
        SerialClear();
        //Serial.write(STK_GET_SYNC);
        Serial.write(CRC_EOP);
        delay(10);
        if (Serial.read() == STK_INSYNC)
        {
          break;
        }
      }

      Wait();
#endif

#ifdef _DEBUG_GetSync_
      Serial.print(F("\r\n"));
      Serial.println(F("[GetSync] End"));
#endif
    }

    //
    //関数: byte GetParameter(byte which)
    //  説明:
    //    optibootからOPTIBOOT_MAJVERまたはOPTIBOOT_MINVERを取得します.
    //
    //  引数:
    //    byte which:
    //      0x82:
    //        OPTIBOOT_MINVER
    //      0x81:
    //        OPTIBOOT_MAJVER
    //
    byte GetParameter(byte which)
    {
      byte ch;

#ifdef _DEBUG_GetParameter_
      Serial.print(F("\r\n"));
      Serial.println(F("[GetParameter] Start"));
      Serial.print(F("[GetParameter] which = "));
      Serial.println(which);
#endif

      SerialClear();
      Serial.write(STK_GET_PARAMETER);
      Serial.write(which);

#ifndef _DEBUG_GetParameter_
      VerifySpace();
      ch = GetCh();
      Wait();
#endif

#ifdef _DEBUG_GetParameter_
      Serial.print(F("\r\n"));
      Serial.println(F("[GetParameter] End"));
#endif
      return ch;
    }

    //
    //関数: void SetAddress(word offset)
    //  説明:
    //    メモリのオフセット値を送信します.
    //
    //  引数:
    //    word offset:
    //      メモリのオフセット値
    //
    void SetAddress(word offset)
    {
      ADDRESS_POS addressPos;
      addressPos.val = offset;

#ifdef _DEBUG_SetAddress_
      Serial.print(F("\r\n"));
      Serial.println(F("[SetAddress] Start"));
      Serial.print(F("{[SetAddress] address_pos.lohi.lo = "));
      Serial.println(addressPos.lohi.lo);
      Serial.print(F("[SetAddress] ]address_pos.lohi.hi = "));
      Serial.println(addressPos.lohi.hi);
#endif

      Serial.write(STK_LOAD_ADDRESS);
      Serial.write(addressPos.lohi.lo);
      Serial.write(addressPos.lohi.hi);

#ifndef _DEBUG_SetAddress_
      VerifySpace();
      Wait();
#endif

#ifdef _DEBUG_SetAddress_
      Serial.print(F("\r\n"));
      Serial.println(F("[SetAddress] End"));
#endif
    }

    //
    //関数: byte SendData(byte num)
    //  説明:
    //    指定する数ぶんのデータをoptibootに送信します.
    //    コマンド"readHexData"と連携します.
    //
    //  引数:
    //    byte num:
    //      送信するデータの数
    //
    //  返り値:
    //    読み込んだデータの数
    //
    byte SendData(byte num)
    {
      int n, m;
      byte size;

#ifdef _DEBUG_SendData_
      Serial.print(F("\r\n"));
      Serial.println(F("[SendData] Start"));

      Serial.print(F("[SendData] ]num = "));
      Serial.println(num);
#endif

      //コマンドの送信
      Serial.write(STK_PROG_PAGE);
      Serial.write(0);
      Serial.write(num);
      Serial.write(70);

      //データを送信
      //BUF_SIZEごとにデータを送信
      for (n = 0; n < num / hexDataBufSize; n++)
      {
        size = ReadHexData(hexDataBufSize);
        for (m = 0; m < hexDataBufSize; m++)
        {
          Serial.write(hexData[m]);

#ifdef _DEBUG_SendData_
          Serial.print(F("$"));
          Serial.print(hexData[m]);
#endif
        }
      }

      //BUF_SIZEで割った時のあまり分のデータを送信
      size = ReadHexData(num % hexDataBufSize);
      for (n = 0; n < num % hexDataBufSize; n++)
      {
        Serial.write(hexData[n]);

#ifdef _DEBUG_SendData_
        Serial.print(F("$"));
        Serial.print(hexData[n]);
#endif

      }

#ifndef _DEBUG_SendData_
      VerifySpace();
      Wait();
#endif

#ifdef _DEBUG_SendData_
      Serial.print(F("\r\n"));
      Serial.println(F("[SendData] End"));
#endif

      return size;
    }

    //
    //関数: boolean SketchWrite(void)
    //  説明:
    //    スケッチをoptibootにかきこませます.
    //
    boolean SketchWrite(void)
    {

#ifdef _DEBUG_SketchWrite_
      Serial.print(F("\r\n"));
      Serial.println(F("[SketchWrite] Start"));
#endif

      //スケッチのリロード
      if (!SketchReload())
      {

#ifdef _DEBUG_SketchWrite_
        Serial.print(F("\r\n"));
        Serial.println(F("[SketchWrite] End"));
#endif
        return false;
      }

      word offset;
      char dataSize;

      offset = 0;

      for (;;)
      {
        //アドレスのオフセット値を送信
        SetAddress(offset);
        dataSize = SendData(sendDataSize);

        //ファイルの最後に来ているときは送信を終える
        if (fileEnded)
        {
          break;
        }

        offset += sendDataSize / 2;
      }

#ifdef _DEBUG_SketchWrite_
      Serial.print(F("\r\n"));
      Serial.println(F("[SketchWrite] End"));
#endif

      return true;
    }

    //
    //関数: void ReadData(void)
    //  説明:
    //    非実装
    //
    void ReadData(void)
    {



    }

    //
    //関数: void SerialClear(void)
    //  説明:
    //    シリアルデータバッファを削除します.
    //
    void SerialClear(void)
    {
      while (Serial.read() != -1);
    }




    //---
    //
    //HexFile操作用関数セット
    //
    //---

    //
    //関数: int StringLength(char *str)
    //	説明:
    //	  文字列の文字数を数えます
    //
    //	引数:
    //	  char *str: 数えたい文字列
    //
    //	戻り値:
    //	  int : 文字数 (ただしNULL文字は含まない)
    //
    int StringLength(char *str)
    {
      int i;

      for (i = 0;; i++)
      {
        if (str[i] == '\0') break;
      }

      return i;
    }

    //
    //関数: SetFileName(char *str)
    //  説明:
    //    文字列strの内容をfile_nameに格納します
    //
    //  引数:
    //    char *str:
    //      格納したい文字列
    //
    void SetFileName(char *str)
    {

      if (str == this->sketchName)
      {
        return;
      }

      //メモリの解放
      free(this->sketchName);

      //メモリの確保
      this->sketchName = (char *)calloc(StringLength(str) + 1, sizeof(char));

      //代入
      byte i;
      for (i = 0; i < StringLength(str) + 1; i++)
      {
        this->sketchName[i] = str[i];
      }
    }

    //
    //関数: boolean SketchLoad(char *sketch_name)
    //  説明:
    //    SDカードに保存されているスケッチが書かれたHexFileを読み込みます.
    //
    //  引数:
    //    char *sketch_name
    //      スケッチが書かれたHEXファイル
    //
    boolean SketchLoad(char *sketchName)
    {

#ifdef _DEBUG_SketchLoad_
      Serial.print(F("\r\n"));
      Serial.println(F("[SketchLoad] Start"));
#endif

      fp = SD.open(sketchName);
      if (!fp)
      {

#ifdef _DEBUG_SketchLoad_
        Serial.print(F("\r\n"));
        Serial.println(F("[SketchLoad] End"));
#endif
        return false;
      }

      //ファイル名を一時保存
      SetFileName(sketchName);

#ifdef _DEBUG_SketchLoad_
      Serial.print(F("sketchName = "));
      Serial.println(this->sketchName);
#endif

      leftOver = 0;
      fileEnded = false;

#ifdef _DEBUG_SketchLoad_
      Serial.print(F("\r\n"));
      Serial.println(F("[SketchLoad] End"));
#endif
      return true;
    }

    //
    //関数: boolean SketchReload(void)
    //  説明:
    //    スケッチをリロードします.
    //
    //  返り値:
    //    true:
    //      成功
    //
    //    false:
    //      失敗
    //
    boolean SketchReload(void)
    {
      SketchClose();

      if (!SketchLoad(this->sketchName))
      {
        return false;
      }
      return true;
    }

    //
    //関数: void SketchClose(void)
    //  説明: 現在開いているHexFileを閉じます.
    //
    void SketchClose(void)
    {
      fp.close();
    }

    //
    //関数: void NextLine(void)
    //  説明:
    //    次の行へ読み取り位置を移動します.
    //    この関数を実行後, read()をすると行先頭の1Byteの値が返ることになります.
    //
    void NextLine(void)
    {
      byte ch;

      for (;;)
      {
        if (fp.peek() == -1)
        {
          return;
        }
        ch = fp.read();
        if (ch == 0x0a || ch == 0x0d)
        {
          ch = fp.peek();
          if (ch == 0x0a || ch == 0x0d)
          {
            fp.read();
            return;
          }
          return;
        }
      }
    }

    //
    //関数: byte CharToValue(char c)
    //  説明:
    //    ASC文字を数値に変換します.
    //
    //  引数:
    //    char c:
    //      変換したい文字コード
    //
    byte CharToValue(char c)
    {
      if (c >= 0x30 && c <= 0x39)
      {
        return c - 0x30;
      }
      if (c >= 0x41 && c <= 0x5a)
      {
        return c - 0x41 + 10;
      }
      if (c >= 0x61 && c <= 0x7a)
      {
        return c - 0x61 + 10;
      }
    }


    //
    //関数: byte JointToOneByte(byte lo, byte hi)
    //  説明:
    //    4bitデータと4bitデータを結合して1Byteデータにします.
    //
    byte JointToOneByte(byte lo, byte hi)
    {
      byte result;
      result = 0x00;

      result |= (hi & 0x0f) << 4;
      result |= lo & 0x0f;

      return result;
    }

    //
    //関数: byte ReadHexVal(void)
    //  説明:
    //    HexFileから1バイトの数値を読み込みます.
    //    つまり,HexFileのテキスト文字を読み込みそれを数値に変換します.
    //
    //  返り値:
    //    読み込んだ数値
    //
    byte ReadHexVal(void)
    {
      int lo;
      int hi;


      hi = fp.read();
      lo = fp.read();

      if (lo != -1 && hi != -1)
      {
        return JointToOneByte(CharToValue((char)lo), CharToValue((char)hi));
      }
      else
      {
        fileEnded = true;
        return 255;
      }
    }


    //
    //関数: byte ReadHexData(byte num)
    //  説明:
    //    HEXファイルから指定した数のデータをデータ配列に格納します.
    //    上限はHEX_DATA_BUF_SIZE までです.
    //    このコマンドがHexFileを主に扱います.
    //
    //  返り値:
    //    読み込んだデータの数
    //
    byte ReadHexData(byte num)
    {

#ifdef _DEBUG_ReadHexData_
      Serial.print(F("\r\n"));
      Serial.println(F("[ReadHexData] Start"));
#endif

      if (num > hexDataBufSize)
      {
        num = hexDataBufSize;
      }

#ifdef _DEBUG_ReadHexData_
      Serial.print(F("[ReadHexData] num = "));
      Serial.println(num);
#endif

      byte i;
      byte dataSizeTotal;
      int ch;
      byte index;

      //書き込み位置を先頭にする
      index = 0;

      for (dataSizeTotal = 0; dataSizeTotal < num && !fileEnded;)
      {
        //読み取り位置が一レコードの途中で終わっているとき
        if (leftOver > 0)
        {
          hexData[index++] = ReadHexVal();
          dataSizeTotal++;
          leftOver--;
        }
        else
        {

          //HexFileの先頭文字':'を探す
          for (;;)
          {
            ch = fp.read();

            //':'がきたとき
            if (ch == ':')
            {
              leftOver = ReadHexVal();

              SkipBytes(6);
              break;
            }
            //データが存在しないとき
            else if (ch == -1)
            {
              leftOver = 0;
              fileEnded = true;
              break;
            }
          }
        }
      }

      //余っている部分には255を代入
      for (i = 0; i < hexDataBufSize - dataSizeTotal; i++)
      {
        hexData[index++] = 255;
      }


#ifdef _DEBUG_ReadHexData_
      Serial.print(F("[ReadHexData] hexData = "));
      PrintArray(hexData, HEX_DATA_BUF_SIZE);
      Serial.print(F("\r\n"));
      Serial.print(F("[ReadHexData] data_size_total = "));
      Serial.println(dataSizeTotal);
      Serial.print(F("\r\n"));
      Serial.println(F("[ReadHexData] End"));
#endif

      return dataSizeTotal;
    }

    //
    //関数: void SkipBytes(byte num)
    //  説明:
    //    任意のByte分読み取り位置をとばします.
    //
    void SkipBytes(byte num)
    {
      byte n;
      for (n = 0; n < num; n++)
      {
        fp.read();
      }
    }

    //
    //関数: void PrintArray(byte *array, byte size)
    //  説明:
    //    指定した配列の内容を表示します.
    //
    //  引数:
    //    byte *array:
    //      配列
    //
    //    byte size:
    //      配列のサイズ
    //
    void PrintArray(byte *array, byte size)
    {
      int i;

      Serial.print(F("{"));
      for (i = 0; i < size; i++)
      {

        Serial.print(array[i]);
        Serial.print(F(", "));
      }
      Serial.print(F("}"));
    }
};

#endif
