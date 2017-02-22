/*
*プログラム: SketchWriter動作テスト用プログラム
*  最終更新日: 4.22.2016
*
*  説明:
*    SketchWriterの動作をテストするためのプログラムです.
*/


#include <SD.h>
#include <SPI.h>

#include "SketchWriter.h"

SketchWriter sketchWriter;
byte flg = 0;

void setup() {
  //ボタン入力ピン設定
  pinMode(3, INPUT);
  sketchWriter.SetResetPin(6);
  
  
  sketchWriter.Begin(10);
}

void loop() {
  int ch;
  ch = 0;
  
  //ボタンが押されるまで待機
  while (digitalRead(3));
  
  if(flg++ % 2 == 0) {
    sketchWriter.SketchLoad("media.hex");
  }
  else {
    sketchWriter.SketchLoad("Blink.hex");
  }
  
  sketchWriter.ResetArduino(6);
  
  sketchWriter.GetSync();
  ch = sketchWriter.GetParameter(0x82);
  sketchWriter.SketchWrite();
  sketchWriter.AppStart();
  
  sketchWriter.SketchClose();
  
  delay(2000);
}
