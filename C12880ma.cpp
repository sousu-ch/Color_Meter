#include "C12880ma.h"

void Spectrum_Data_raw::update_max()
{
    max_value = 0;
    for (int i = 0; i < PIXCEL_NUM; i++)
    {
        if (data[i] > max_value)
        {
            max_value = data[i];
        }
    }
}

//クロックのようなものを作るメソッド
void C12880MA::clock_high_low(int pin_num)
{
    digitalWrite(pin_num, 1);
    delayMicroseconds(delay_time_microsecond);
    digitalWrite(pin_num, 0);
}

// RGBそれぞれのガンマ補正をするメソッド
uint8_t C12880MA::gamma_correction(double RGB_value)
{
    double tmp_value;
    //ガンマ補正
    if (RGB_value < 0.0031308)
    {
        tmp_value = 12.92 * RGB_value;
    }
    else
    {
        tmp_value = 1.055 * pow(RGB_value, (1 / 2.4)) - 0.055;
    }
    //値を0～1に修正する
    if (tmp_value > 1)
    {
        tmp_value = 1.0;
    }
    else if (tmp_value < 0)
    {
        tmp_value = 0;
    }
    return uint8_t(tmp_value * 255);
}

// XYZ→RGB変換、ガンマ補正をして、16bitのカラーコードを返すメソッド
uint16_t C12880MA::XYZ_to_Color(double X, double Y, double Z)
{
    uint16_t color_value = TFT_BLACK; //例外になる領域は、とりあえず黒で描画する
    // XYZ→xyzの変換を行う
    double sum_XYZ = X + Y + Z;
    if (sum_XYZ != 0)
    {
        double x = X / sum_XYZ;
        double y = Y / sum_XYZ;
        double z = Z / sum_XYZ;
        if (y != 0)
        { // xyz→XYZの変換を行う
            X = x / y;
            Y = 1.0;
            Z = z / y;
            // xyz→RGBの変換を行う
            uint8_t R = gamma_correction(3.241 * X - 1.5374 * Y - 0.4986 * Z);
            uint8_t G = gamma_correction(-0.9692 * X + 1.876 * Y + 0.0416 * Z);
            uint8_t B = gamma_correction(0.0556 * X - 0.204 * Y + 1.057 * Z);
            color_value = (((R >> 3) << 11) | (G >> 2) << 5 | B >> 3); // R、Bを5bit、Gを6bitになるようビットシフトする
        }
    }
    return color_value;
}

//画素の位置から波長を計算するメソッド。f(pix)=λ
int C12880MA::calc_lamda(int pix_num)
{
    //画素数から波長を計算する。最後の「+0.5」は、値を四捨五入して整数値を返すため。
    return int(pA0 + pB1 * pix_num + pB2 * pow(pix_num, 2.0) + pB3 * pow(pix_num, 3.0) + pB4 * pow(pix_num, 4.0) + pB5 * pow(pix_num, 5.0) + 0.5);
}

//画素の位置から波長を計算するメソッド。f(pix)=λ
double C12880MA::calc_dlamda(int pix_num)
{
    return double(pA0 + pB1 * pix_num + pB2 * pow(pix_num, 2.0) + pB3 * pow(pix_num, 3.0) + pB4 * pow(pix_num, 4.0) + pB5 * pow(pix_num, 5.0));
}

//隣接する画素との波長の差(みたいなもの)dλ/d(pix)をもとに重みを求めるメソッド
//画素の位置によってセンサの波長方向の分解能が変わるので、画素と波長の関数を画素方向で微分して重みとして扱う。
double C12880MA::calc_weight(int pix_num)
{
    return 1.0 / (pB1 + 2.0 * pB2 * pix_num + 3.0 * pB3 * pow(pix_num, 2.0) + 4.0 * pB4 * pow(pix_num, 3.0) + 5.0 * pB5 * pow(pix_num, 4.0));
}

//スペクトル(分光分布)から色を計算するメソッド
void C12880MA::calc_measured_color(Spectrum_Data_raw &data)
{
    //以下の処理で例外処理に該当する場合、色はとりあえず黒にしておく
    data.color = TFT_BLACK;
    data.R = 0;
    data.G = 0;
    data.B = 0;

    // XYZの値を計算する
    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;

    //等色関数とスペクトルの測定値と重みをXYZについてそれぞれ掛け合わせて足す
    //なお、dλ/d(pix)を重みとする。
    for (int i = 0; i < PIXCEL_NUM; i++)
    {
        if (i_lamdas[i] >= 390 and i_lamdas[i] <= 830)
        { //可視光領域だったら
            X += calc_weight(i) * data.data[i] * CMF_390_830_X[i_lamdas[i] - 390];
            Y += calc_weight(i) * data.data[i] * CMF_390_830_Y[i_lamdas[i] - 390];
            Z += calc_weight(i) * data.data[i] * CMF_390_830_Z[i_lamdas[i] - 390];
        }
    }

    // XYZ→xyzの変換を行う
    double sum_XYZ = X + Y + Z;
    if (sum_XYZ != 0)
    {
        double x = X / sum_XYZ;
        double y = Y / sum_XYZ;
        double z = Z / sum_XYZ;
        if (y != 0)
        { // xyz→XYZの変換を行う
            X = x / y;
            Y = 1.0;
            Z = z / y;
            // xyz→RGBの変換を行う
            data.R = gamma_correction(3.241 * X - 1.5374 * Y - 0.4986 * Z);
            data.G = gamma_correction(-0.9692 * X + 1.876 * Y + 0.0416 * Z);
            data.B = gamma_correction(0.0556 * X - 0.204 * Y + 1.057 * Z);
            data.color = (((data.R >> 3) << 11) | (data.G >> 2) << 5 | data.B >> 3); // R、Bを5bit、Gを6bitになるようビットシフトする
        }
    }
}

//単波長の光の色を計算するメソッド。画素位置を入力とする
uint16_t C12880MA::calc_color(int pix_num)
{
    uint16_t color_value = TFT_BLACK; //可視光領域外の部分は、とりあえず黒で描画する
    if (i_lamdas[pix_num] >= 390 and i_lamdas[pix_num] <= 830)
    { //その画素位置が可視光領域だったら
        //等色関数から単波長の光の色(XYZ表色系)を計算する
        double X = CMF_390_830_X[i_lamdas[pix_num] - 390];
        double Y = CMF_390_830_Y[i_lamdas[pix_num] - 390];
        double Z = CMF_390_830_Z[i_lamdas[pix_num] - 390];
        color_value = XYZ_to_Color(X, Y, Z);
    }
    return color_value;
}

//コンストラクタ。CLK、ST、及びVideo信号の接続されたピン番号、及びC12880MA購入時に付属している、検査成績書の値を記入すること。
C12880MA::C12880MA(int _CLK_pin, int _ST_pin, int _Video_pin, double _pA0, double _pB1, double _pB2, double _pB3, double _pB4, double _pB5)
{
    CLK_pin = _CLK_pin;
    ST_pin = _ST_pin;
    Video_pin = _Video_pin;
    pinMode(CLK_pin, OUTPUT); // CLKのピンを出力モードにする
    pinMode(ST_pin, OUTPUT);  // STのピンを出力モードにする

    pA0 = _pA0;
    pB1 = _pB1;
    pB2 = _pB2;
    pB3 = _pB3;
    pB4 = _pB4;
    pB5 = _pB5;

    for (int i = 0; i < PIXCEL_NUM; i++)
    {
        i_lamdas[i] = calc_lamda(i); //画素位置と波長の関係を調べておく
        d_lamdas[i] = calc_dlamda(i); //画素位置と波長の関係を調べておく
    }
    Serial.begin(115200); //デバッグ用
}

//スペクトルを測定するメソッド。引数にSpectrum_Data_raw型のオブジェクトを指定して、スペクトルの配列と色情報を測定・格納する。
void C12880MA::read_data(Spectrum_Data_raw &data)
{
    const int thp_ST = 6;       //スタートパルスhigh期間。6以上なら何でもOK
    const int Video_delay = 87; // Video取り込み開始までのクロック数
    digitalWrite(ST_pin, 1);    // STをhighにする
    for (int i = 0; i < (thp_ST); i++)
    { //スタートパルスhigh期間分クロック
        clock_high_low(CLK_pin);
    }
    digitalWrite(ST_pin, 0); // STをlowにする
    for (int i = 0; i < (Video_delay); i++)
    { // Video取り込み開始までのクロック数分クロック
        clock_high_low(CLK_pin);
    }

    for (int i = 0; i < (PIXCEL_NUM); i++)
    { //ここからVideo信号を取り込む
        clock_high_low(CLK_pin);
        data.data[i] = analogRead(Video_pin);//-420;

        //M5Stackの36pinの電圧-ADC特性は、2.5V-約2850からリニアリティーが悪化する。
        //したがって、使用するADC入力電圧範囲を制限する必要がある。
        //ここでは、内部的に2500で飽和させ、デバッグ的に異常値が入っていることが分かるようにした。
        //実使用ではOPアンプ側の回路で2.5は入力されないように設計したので、何かの異常があるときに限り飽和することになる。
        if (data.data[i] > 2500)
            data.data[i] = 0;

        if (data.data[i]<0)
            data.data[i]=0;
    }
    //補正値適用は平均化処理のあとにするので、ここでは適用しないようにした
#if 0
    for (int i = 0; i < (PIXCEL_NUM); i++)
    { //読み込んだデータを、補正する
        if ((i_lamdas[i] >= 340) and (i_lamdas[i] <= 850))
        {
            data.data[i] = (int)(data.data[i] * 100 / Relative_Sensitivity_340_850[i_lamdas[i] - 340]);
        }
    }
#endif
    data.update_max();
    //測定結果を使用して、色を計算する
    calc_measured_color(data);
}

//蓄積時間を取得するメソッド
int C12880MA::get_delay_time_microsecond()
{
    return delay_time_microsecond;
}
//蓄積時間をセットするメソッド
void C12880MA::set_delay_time_microsecond(int value)
{
    delay_time_microsecond = value;
}
