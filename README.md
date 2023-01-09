Color Meter using M5Stack
====
 
 M5Stackを使用した分光方式カラーメーター スペクトロメーター

## Description
M5Stackと分光センサー「C12880MA」の組み合わせで、照明のスペクトル表示や演色評価数を表示できます  
解説:  http://sousuch.web.fc2.com/DIY/cm/

## Example
![Examples](https://github.com/sousu-ch/Color_Meter/blob/master/gaikan.jpg "外観")

## Requirement
ArduinoでM5Stack.hを使用しています  
また、画像を１枚表示するため、SPIFFSを使用しています。

## Usage
FTDIのシリアル変換モジュールおよび測定対象を、あらかじめUSBに接続した状態で、HID_Latency.exeを起動します。  
動作としては下記ような流れになります。

1. 最初にオフセットキャリブレーション（センサー遮蔽状態のオフセット検出）を実行します
2. あとは、１回測定もくしは１６回平均測定ボタンで測定すると、LCDにスペクトルや演色評価が表示されます
3. PCにUSBで接続すれば、シリアルでAD生データやスペクトルデータ等が出力されますので、PC側で精密な解析もできます


...

## Install
ArduinoからM5Stackをボードマネージャーから選択して使用してください  
dataフォルダ以下のxy.pngをSPIFFSでアップロードする必要があります。
## Licence
The source code is licensed MIT.
