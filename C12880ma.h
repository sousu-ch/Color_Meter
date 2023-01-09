#ifndef _C12880MA_H_
#define _C12880MA_H_

#include <M5Stack.h>
#include <math.h>
#include "CMF.h"

using std::pow;
#define PIXCEL_NUM 288 // C12880MAの画素数

//測定結果を格納するための構造体
struct Spectrum_Data_raw
{
    uint8_t R, G, B;
    uint16_t color;
    int data[PIXCEL_NUM];
    int max_value;
    void update_max();
};

//測定結果を格納するための構造体
struct Spectrum_Data
{
    double data[95];         // 360nm to 830nm のスペクトラムデータ : 5nm STEP
    uint16_t color_temp;     // 相対色温度[K]
    double X, Y, Z;          // X10Y10Z1の３刺激値
    double x, y, z;          // X10Y10Z1の３刺激値を正規化したもの
    double K;                // 3刺激値用補正係数
    double k;                // 3刺激値用補正係数
    double Xi[15], Yi[15], Zi[15]; // X10Y10Z1の３刺激値
    double xi[15], yi[15], i[15]; // X10Y10Z1の３刺激値

    // CIE1960UCS色度図上の色度座標u,v
    double u, v;             // 光源色の色度座標
    double ud, vd;           // 色順応補正後の光源色の色度座標
    double c, d;             // 光源色の色度座標から求める係数
    double ui[15], vi[15];   // 光源色による各試験色の色度座標
    double udi[15], vdi[15]; // 色順応補正後の光源色による各試験色の色度座標
    double ci[15], di[15];   // 光源色による各試験色の色度座標から求める係数

    // CIE1964均等色空間上の色度座標w,u,v
    double w_CIE1964[15], u_CIE1964[15], v_CIE1964[15];

    double ri[15];           //Ri:特殊演色評価数
    double ra;               // Ra:平均演色評価数
};

//分光光度センサC12880MAを扱うためのクラス
class C12880MA
{
private:
    double pA0, pB1, pB2, pB3, pB4, pB5; //波長補正係数
    int delay_time_microsecond = 3;     //クロック幅[μs]だいたい、delay_time_microsecond＋１usが周期になる
    int CLK_pin, ST_pin, Video_pin;      //クロックとスタートパルスとビデオ信号のピン番号

    void clock_high_low(int pin_num);                    //クロックのようなものを作るメソッド
    uint8_t gamma_correction(double RGB_value);          // RGBそれぞれのガンマ補正をするメソッド
    int calc_lamda(int pix_num);                         //画素の位置から波長を計算するメソッド。f(pix)=λ
    double calc_dlamda(int pix_num);                         //画素の位置から波長を計算するメソッド。f(pix)=λ
    double calc_weight(int pix_num);                     //隣接する画素との波長の差(みたいなもの)dλ/d(pix)をもとに重みを求めるメソッド
    void calc_measured_color(Spectrum_Data_raw &data);   //スペクトル(分光分布)から色を計算するメソッド
    void conv_raw_to_step5(Spectrum_Data_raw &data);     //スペクトル(分光分布)から色を計算するメソッド
public:
    uint16_t XYZ_to_Color(double X, double Y, double Z); // XYZ→RGB変換、ガンマ補正をして、16bitのカラーコードを返すメソッド
    int i_lamdas[PIXCEL_NUM];                            //各画素位置に対応した光の波長の配列
    double d_lamdas[PIXCEL_NUM]; //各画素位置に対応した光の波長の配列

    //コンストラクタ。CLK、ST、及びVideo信号の接続されたピン番号、及びC12880MA購入時に付属している、検査成績書の値を記入すること。
    C12880MA(int _CLK_pin, int _ST_pin, int _Video_pin, double _pA0, double _pB1, double _pB2, double _pB3, double _pB4, double _pB5);

    //スペクトルを測定するメソッド。引数にSpectrum_Data_raw型のオブジェクトを指定して、スペクトルの配列と色情報を測定・格納する。
    void read_data(Spectrum_Data_raw &data);

    uint16_t calc_color(int pix_num);           //単波長の光の色を計算するメソッド。画素位置を入力とする
    int get_delay_time_microsecond();           //蓄積時間を取得する関数
    void set_delay_time_microsecond(int value); //蓄積時間をセットする関数
};

#endif // _C12880MA_H_