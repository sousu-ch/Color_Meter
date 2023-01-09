#include <M5Stack.h>
#include "C12880ma.h"

//測定の平均回数設定
#define AVERAGE_COUNT_A 1 //ボタンAを押したときの平均回数
#define AVERAGE_COUNT_B 16      //ボタンBを押したときの平均回数
#define AVERAGE_COUNT_OFFSET 16 //ボタンCを押したときのOFFSET平均回数


// C12880MAの波長補正係数を設定。
//各自のC12880MAに合わせて、購入時に付属している検査成績書の値を記入すること。
double pA0 = 3.125870124E+02;
double pB1 = 2.685351313;
double pB2 = -1.037645810E-03;
double pB3 = -8.595055360E-06;
double pB4 = 1.296488366E-08;
double pB5 = -2.291425284E-13;

//画面の描画などに関わる値の定義
#define SPECTRUM_PIXCEL_NUM_X 300 // X軸サイズ
#define SPECTRUM_PIXCEL_NUM_Y 100 // Y軸サイズ

//コンストラクタ。CLK、ST、及びVideo信号の接続されたピン番号、及びC12880MA購入時に付属している、検査成績書の値を記入すること。
C12880MA c12880ma(16, 17, 36, pA0, pB1, pB2, pB3, pB4, pB5);

struct Spectrum_Data_raw Ref, Trans;   //参照光と透過光に関する情報を格納するためのオブジェクト
struct Spectrum_Data R, K;             //基準の光と試験光の情報を格納するためのオブジェクト
int delay_time_microsecond;            // Clock幅。この値を変更することで、蓄積時間を制御する。
#define MAX_Delay_Time_Microsecond 300 //クロック幅の最大値
#define MIN_Delay_Time_Microsecond 3   //クロック幅の最小値
#define Step_Ratio_Delay_Time 10       //クロック幅の増加比率
#define Step_Delay_Time 1              //クロック幅の増加数

uint32_t databuf[PIXCEL_NUM];
double databuf_lcdy[SPECTRUM_PIXCEL_NUM_X]; //表示するスペクトルグラフの、相対ピクセル位置に対応したスペクトル値
double databuf_lcdy_max;                    //表示するスペクトル値の最大値。最大値で正規化して表示するため。
double offset_arr[PIXCEL_NUM];              //センサーから受信する値はAD値で200前後のオフセットがある
double t_per_pix;                           //スペクトラムエリアの横軸に対する測定波長範囲

char s[255]; //画面表示用テキストバッファ。sprintfで使用する

//測定結果をLCDに表示する
void update_screen()
{
    //    M5.Lcd.fillRect(0, 0, 320, 240, TFT_BLACK); //グラフエリアの塗りつぶし　スタート座標と塗りつぶしピクセル数
    M5.Lcd.fillScreen(TFT_BLACK); //結果表示のため、画面全体を塗りつぶし

    //スペクトル塗りつぶし
    for (int i = 0; i < SPECTRUM_PIXCEL_NUM_X; i++)
    {
        uint16_t freq = uint16_t((c12880ma.d_lamdas[0] + t_per_pix * i) + 0.5); //波長範囲（購入時のデータから出す）を使って、ピクセル位置の周波数を計算する
                                                                                //        uint16_t color_value = TFT_BLACK; //可視光領域外の部分は、とりあえず黒で描画する

        //可視光範囲を色付けする
        if (freq >= 390 && freq <= 830)
        {
            double X = CMF_390_830_X[freq - 390];
            double Y = CMF_390_830_Y[freq - 390];
            double Z = CMF_390_830_Z[freq - 390];
            uint16_t color = c12880ma.XYZ_to_Color(X, Y, Z);
            M5.Lcd.drawLine(14 + i, 240 - 11 - (SPECTRUM_PIXCEL_NUM_Y / databuf_lcdy_max * databuf_lcdy[i]), 14 + i, 240 - 11, color);
        }
    }

    //スペクトル背景の罫線横
    for (int i = 0; i < 9; i++)
    {
        M5.Lcd.drawLine(12, 139 + i * 10, 319, 139 + i * 10, TFT_DARKGREY);
    }
    //スペクトル背景の罫線縦
    for (int i = 0; i < 11; i++)
    {
        M5.Lcd.drawLine(14 + (350 + 50 * i - c12880ma.d_lamdas[0]) / t_per_pix, 129, 14 + (350 + 50 * i - c12880ma.d_lamdas[0]) / t_per_pix, 228, TFT_DARKGREY);
    }

    //スペクトル軌跡。軌跡は罫線の上に表示したいので、あとから描画する
    for (int i = 0; i < SPECTRUM_PIXCEL_NUM_X - 1; i++)
    {
        M5.Lcd.drawLine(14 + i, 240 - 11 - (SPECTRUM_PIXCEL_NUM_Y / databuf_lcdy_max * databuf_lcdy[i]),
                        14 + i + 1, 240 - 11 - (SPECTRUM_PIXCEL_NUM_Y / databuf_lcdy_max * databuf_lcdy[i + 1]), TFT_WHITE);
    }

    //特殊演色評価目盛り線
    for (int i = 0; i < 9; i++)
    {
        M5.Lcd.drawLine(12, 21 + i * 10, 224, 21 + i * 10, TFT_DARKGREY);
    }

    //特殊演色評価縦棒グラフ
    for (int i = 0; i < 16; i++)
    {
        M5.Lcd.fillRect(17 + 13 * i, 243 - 131 - int(K.ri[i]), 9, int(K.ri[i]), ricolor[i]); //グラフエリアの塗りつぶし　スタート座標と塗りつぶしピクセル数
    }

    // XY色度図背景。SPIFFSに置いたファイルで、画像内に罫線も含まれている。
    M5.Lcd.drawPngFile(SPIFFS, "/xy.png", 237, 17);

    // xy色度グラフ。該当xy座標位置に黒の十字を表示する
    M5.Lcd.drawLine(238 + 110 * K.x - 1, 111 - 110 * K.y, 238 + 110 * K.x + 1, 111 - 110 * K.y, TFT_BLACK);
    M5.Lcd.drawLine(238 + 110 * K.x, 111 - 110 * K.y - 1, 238 + 110 * K.x, 111 - 110 * K.y + 1, TFT_BLACK);
    // M5.Lcd.drawLine(10, 8, 10, 11, TFT_BLACK);

    //特殊演色評価軸線
    M5.Lcd.drawLine(12, 9, 12, 111, TFT_LIGHTGREY);
    M5.Lcd.drawLine(12, 111, 224, 111, TFT_LIGHTGREY);

    //スペクトル軸線
    M5.Lcd.drawLine(12, 129, 12, 229, TFT_LIGHTGREY);
    M5.Lcd.drawLine(12, 229, 319, 229, TFT_LIGHTGREY);

    //ここから軸目盛等の文字表示が出てくるので、色設定する
    M5.Lcd.setTextColor(TFT_DARKGREY, TFT_BLACK); //文字色設定と背景色設定
    M5.Lcd.setTextFont(1);                        //最小フォントでみっちり表示する

    //スペクトル横軸目盛り
    for (int i = 0; i < 11; i++)
    {
        M5.Lcd.setCursor(26 + i * 26, 231);
        M5.Lcd.printf("%3d", 350 + i * 50);
    }
    M5.Lcd.setCursor(307, 231);
    M5.Lcd.printf("nm");

    //演色評価横軸（項目軸）目盛り
    for (int i = 0; i < 9; i++)
    {
        M5.Lcd.setCursor(13 + i * 13, 114);
        M5.Lcd.printf("%2d", i + 1);
    }
    for (int i = 9; i < 15; i++)
    {
        M5.Lcd.setCursor(16 + i * 13, 114);
        M5.Lcd.printf("%2d", i + 1);
    }
    M5.Lcd.setCursor(16 + 15 * 13, 114);
    M5.Lcd.printf("RA");

    for (int i = 0; i < 10; i++)
    {
        //演色評価縦軸目盛り
        M5.Lcd.setCursor(0, 109 - i * 10);
        M5.Lcd.printf("%2d", i * 10);
        //スペクトル縦軸目盛り
        M5.Lcd.setCursor(0, 225 - i * 10);
        M5.Lcd.printf("%02d", i);
        M5.Lcd.drawPixel(5, 232 - i * 10, TFT_DARKGREY); //小数点
    }
    //  ドット絵で特殊演色評価の軸の値の"100"を表示する（そのままではぎりぎり3桁表示できなかった）
    M5.Lcd.drawLine(0, 10, 0, 15, TFT_DARKGREY);
    M5.Lcd.drawLine(2, 11, 2, 14, TFT_DARKGREY);
    M5.Lcd.drawLine(3, 10, 4, 10, TFT_DARKGREY);
    M5.Lcd.drawLine(3, 15, 4, 15, TFT_DARKGREY);
    M5.Lcd.drawLine(5, 11, 5, 14, TFT_DARKGREY);
    M5.Lcd.drawLine(7, 11, 7, 14, TFT_DARKGREY);
    M5.Lcd.drawLine(8, 10, 9, 10, TFT_DARKGREY);
    M5.Lcd.drawLine(8, 15, 9, 15, TFT_DARKGREY);
    M5.Lcd.drawLine(10, 11, 10, 14, TFT_DARKGREY);

    //スペクトル縦軸目盛り1.0
    M5.Lcd.setCursor(0, 225 - 10 * 10);
    M5.Lcd.printf("%02d", 10);

    //スペクトル縦軸目盛り小数点
    M5.Lcd.drawPixel(5, 232 - 10 * 10, TFT_DARKGREY); //小数点

    // xy色度図縦軸目盛り
    for (int i = 0; i < 9; i++)
    {
        M5.Lcd.setCursor(226, 109 - i * 11);
        M5.Lcd.printf(".%d", i);
    }
    // xy色度図横軸目盛り
    for (int i = 0; i < 7; i++)
    {
        M5.Lcd.setCursor(240 + i * 11, 115);
        M5.Lcd.printf(".%d", i + 1);
    }

    //演色評価結果表示
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK); //文字色設定と背景色設定。結果は目立たせるため明るく表示する
    for (int i = 0; i < 15; i++)
    {
        M5.Lcd.setCursor(16 + i * 13, 0);
        M5.Lcd.printf("%2d", int(K.ri[i])); // 100を表示できないので２桁で切り捨て。JIS Z 8726:1990 によると、JIS Z 8401が指定されている。
    }

    M5.Lcd.setCursor(16 + 15 * 13, 0);
    M5.Lcd.printf("%2d", int(K.ra));

    M5.Lcd.setCursor(234, 0);
    M5.Lcd.printf("RA:%2d", int(K.ra)); // 100を表示できないので２桁で切り捨て。JIS Z 8726:1990 によると、JIS Z 8401が指定されている。
    M5.Lcd.setCursor(271, 0);
    M5.Lcd.printf("T:%dK", K.color_temp);
    M5.Lcd.setCursor(271, 10);
    M5.Lcd.printf("x:%.3f", K.x); // JIS Z 8726:1990によると、演色評価数の付記事項として、評価した試料光源の色度座標x，yを小数点以下3けた目まで付記する。
    M5.Lcd.setCursor(271, 20);
    M5.Lcd.printf("y:%.3f", K.y);
}

void init_screen()
{
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK); //文字色設定と背景色設定
    M5.Lcd.setTextFont(4);                     //最小
    M5.Lcd.setCursor(60, 60);
    M5.Lcd.printf("COLOR METER");
    M5.Lcd.setCursor(110, 90);
    M5.Lcd.printf("Ver.1.0");
    M5.Lcd.setCursor(10, 150);
    M5.Lcd.printf("Run the OFFSET CAL. first.");
    M5.Lcd.setCursor(220, 200);
    M5.Lcd.printf("OFFSET");
    //表示範囲の正規化。スペクトラム表示エリアの表示範囲を測定波長範囲全体とした
    t_per_pix = (c12880ma.d_lamdas[PIXCEL_NUM - 1] - c12880ma.d_lamdas[0]) / SPECTRUM_PIXCEL_NUM_X;
}

void calc_CRI(void)
{

    uint16_t color_temp = 0;
    uint16_t ctempbuf = 0;
    double dist = 0;
    double dist_m = 1;
    uint8_t t;

    //試料光源の三刺激値の計算
    K.X = 0;
    K.Y = 0;
    K.Z = 0;
    for (t = 0; t < 95; t++)
    {
        K.X = K.X + K.data[t] * CMF2deg_360_830_X[t];
        K.Y = K.Y + K.data[t] * CMF2deg_360_830_Y[t];
        K.Z = K.Z + K.data[t] * CMF2deg_360_830_Z[t];
    }
    //試料光源の色度座標の計算
    K.K = 100 / K.Y;
    K.X = K.K * K.X;
    K.Y = K.K * K.Y;
    K.Z = K.K * K.Z;
    K.u = 4 * K.X / (K.X + 15 * K.Y + 3 * K.Z);
    K.v = 6 * K.Y / (K.X + 15 * K.Y + 3 * K.Z);

    K.k = K.X + K.Y + K.Z;
    K.x = K.X / K.k;
    K.y = K.Y / K.k;
    K.z = K.Z / K.k;
    //    K.u = 4 * K.x / (-2 * K.x + 12 * K.y + 3);
    //    K.v = 6 * K.y / (-2 * K.x + 12 * K.y + 3);

    // 2.相対色温度を計算する
    //ここでは使用する波長範囲を360nm to 830nm 5nm間隔としている。周波数範囲はCIE方式？（後で確認★★★★）
    //参考：JIS Z 8725：2015は400 nm to 750 nm 10 nm間隔

    // CIE1960UCSの色度座標において，黒体放射軌跡上の
    //最も近い点の黒体温度T（単位はＫ）を，相関色温度と呼ぶ。
    //黒体の色度はaiの式で求め、距離を温度tを変えながら繰り返し計算して
    //最も距離の絶対値が小さくなる温度tが相関色温度である。

    //速度的にかなり重い処理ので、100K単位で間引き計算してから±100K範囲を総当たりした
    //速度重視するなら、JIS Z 8725準拠でテーブル参照するのが早いと思う
    for (int ctemp = 0; ctemp < 25000; ctemp += 100)
    {
        double ax = 0;
        double ay = 0;
        double az = 0;
        double ai = 0;
        double au = 0;
        double av = 0;

        for (t = 0; t < 95; t++)
        {

            ai = 1 / (pow(((360 + t * 5) * 0.000000001), 5) * (exp(0.01438775 / ((360 + t * 5) * 0.000000001 * ctemp)) - 1));
            ax += ai * CMF2deg_360_830_X[t];
            ay += ai * CMF2deg_360_830_Y[t];
            az += ai * CMF2deg_360_830_Z[t];
        }
        double Kt = 100 / ay;
        ax = Kt * ax;
        ay = Kt * ay;
        az = Kt * az;
        au = 4 * ax / (ax + 15 * ay + 3 * az);
        av = 6 * ay / (ax + 15 * ay + 3 * az);

        dist = pow((K.u - au), 2) + pow((K.v - av), 2);
        if (dist < dist_m)
        {
            dist_m = dist;
            color_temp = ctemp;
        }
    }
    dist_m = 1;
    ctempbuf = color_temp;
    for (int ctemp = ctempbuf - 100; ctemp < ctempbuf + 100; ctemp++)
    {
        double ax = 0;
        double ay = 0;
        double az = 0;
        double ai = 0;
        double au = 0;
        double av = 0;

        for (t = 0; t < 95; t++)
        {

            ai = 1 / (pow(((360 + t * 5) * 0.000000001), 5) * (exp(0.01438775 / ((360 + t * 5) * 0.000000001 * ctemp)) - 1));
            ax += ai * CMF2deg_360_830_X[t];
            ay += ai * CMF2deg_360_830_Y[t];
            az += ai * CMF2deg_360_830_Z[t];
        }
        double Kt = 100 / ay;
        ax = Kt * ax;
        ay = Kt * ay;
        az = Kt * az;

        au = 4 * ax / (ax + 15 * ay + 3 * az);
        av = 6 * ay / (ax + 15 * ay + 3 * az);
        dist = pow((K.u - au), 2) + pow((K.v - av), 2);

        if (dist < dist_m)
        {
            dist_m = dist;
            K.color_temp = ctemp;
        }
    }

    // 3.基準の光の相対分光分布を計算する
    double xD, yD, M1, M2, M3;
    // 5000K未満は完全放射体の光を基準の光とする(CIE13.3 ,JIS Z 8726 1990)
    if (K.color_temp < 5000)
    {
        for (t = 0; t < 95; t++)
        {
            // Sr(i) = (c1 * ((nm(i) * 0.000000001) ^ (-5)) * (2.718282 ^ (c2 * ((nm(i) * 0.000000001 * t) ^ (-1))) - 1) ^ (-1))
            R.data[t] = 1 / (pow(((360 + t * 5) * 0.000000001), 5) * (exp(0.01438775 / ((360 + t * 5) * 0.000000001 * K.color_temp)) - 1));
        }
    }
    // 5000K以上はCIE昼光を基準の光にする
    else
    {
        if (K.color_temp > 4000 && K.color_temp <= 7000)
        {
            xD = -4.607 * (1E9 / pow(K.color_temp, 3)) + 2.9678 * (1E6 / pow(K.color_temp, 2)) + 0.09911 * (1E3 / K.color_temp) + 0.244063;
        }
        else if (t > 7000) //<=25000Kまでという話もあるみたい。このプログラムでは、演算速度の関係で相対色温度の計算を10000Kまでしか実装していないので、制限しない。
        {
            xD = -2.0064 * (1E9 / pow(K.color_temp, 3)) + 1.9018 * (1E6 / pow(K.color_temp, 2)) + 0.24748 * (1E3 / K.color_temp) + 0.23704;
        }
        else
        {
            //色温度4000以下はCIE昼光の定義がない。黒体で近似できるためらしい。CIE13.3 ,JIS Z 8726 1990では5000Kを境目にしているので、ここは通らない
        }

        yD = -3 * xD * xD + 2.87 * xD - 0.275;
        M1 = (-1.3515 - 1.7703 * xD + 5.9114 * yD) / (0.0241 + 0.2562 * xD - 0.7341 * yD);
        M2 = (0.03 - 31.4424 * xD + 30.0717 * yD) / (0.0241 + 0.2562 * xD - 0.7341 * yD);
        for (t = 0; t < 95; t++)
        {
            R.data[t] = SPD_S1[t] + M1 * SPD_S2[t] + M2 * SPD_S3[t];
        }
    }

    //波長560 nmで正規化する
    double R560 = R.data[40];
    for (t = 0; t < 95; t++)
    {
        R.data[t] = R.data[t] / R560;
    }

    // 4.試料光源および基準光源の光源色を計算する

    //基準の光のの三刺激値の計算
    R.X = 0;
    R.Y = 0;
    R.Z = 0;
    for (t = 0; t < 95; t++)
    {
        R.X = R.X + R.data[t] * CMF2deg_360_830_X[t];
        R.Y = R.Y + R.data[t] * CMF2deg_360_830_Y[t];
        R.Z = R.Z + R.data[t] * CMF2deg_360_830_Z[t];
    }
    R.K = 100 / R.Y;
    R.X = R.K * R.X;
    R.Y = R.K * R.Y;
    R.Z = R.K * R.Z;

    //基準の光の色度座標の計算
    R.u = 4 * R.X / (R.X + 15 * R.Y + 3 * R.Z);
    R.v = 6 * R.Y / (R.Y + 15 * R.Y + 3 * R.Z);

    // xyから求める方法もあるが、今回は使わない
    /*
        R.k = R.X + R.Y + R.Z;
        R.x = R.X / R.k;
        R.y = R.Y / R.k;
        R.z = R.Z / R.k;
        R.u = 4 * R.x / (-2 * R.x + 12 * R.y + 3);
        R.v = 6 * R.y / (-2 * R.x + 12 * R.y + 3);
     */

    for (uint8_t i = 0; i < 15; i++)
    {

        // 5.試験色の三刺激値および色度座標の計算
        //試験色の試料光源による三刺激値の計算
        K.Xi[i] = 0;
        K.Yi[i] = 0;
        K.Zi[i] = 0;
        for (t = 0; t < 95; t++)
        {
            K.Xi[i] = K.Xi[i] + K.data[t] * CMF2deg_360_830_X[t] * CIE_TCS14plus1_360_830[i][t];
            K.Yi[i] = K.Yi[i] + K.data[t] * CMF2deg_360_830_Y[t] * CIE_TCS14plus1_360_830[i][t];
            K.Zi[i] = K.Zi[i] + K.data[t] * CMF2deg_360_830_Z[t] * CIE_TCS14plus1_360_830[i][t];
        }
        K.Xi[i] = K.K * K.Xi[i];
        K.Yi[i] = K.K * K.Yi[i];
        K.Zi[i] = K.K * K.Zi[i];

        //試験色の試料光源による色度座標の計算
        K.ui[i] = 4 * K.Xi[i] / (K.Xi[i] + 15 * K.Yi[i] + 3 * K.Zi[i]);
        K.vi[i] = 6 * K.Yi[i] / (K.Xi[i] + 15 * K.Yi[i] + 3 * K.Zi[i]);

        /*
                K.k = K.Xi[i] + K.Yi[i] + K.Zi[i];
                K.xi[i] = K.Xi[i] / K.k;
                K.yi[i] = K.Yi[i] / K.k;
                K.zi[i] = K.Zi[i] / K.k;
                K.ui[i] = 4 * K.xi[i] / (-2 * K.xi[i] + 12 * K.yi[i] + 3);
                K.vi[i] = 6 * K.yi[i] / (-2 * K.xi[i] + 12 * K.yi[i] + 3);
         */

        //試験色の基本の光による三刺激値の計算
        R.Xi[i] = 0;
        R.Yi[i] = 0;
        R.Zi[i] = 0;
        for (t = 0; t < 95; t++)
        {
            R.Xi[i] = R.Xi[i] + R.data[t] * CMF2deg_360_830_X[t] * CIE_TCS14plus1_360_830[i][t];
            R.Yi[i] = R.Yi[i] + R.data[t] * CMF2deg_360_830_Y[t] * CIE_TCS14plus1_360_830[i][t];
            R.Zi[i] = R.Zi[i] + R.data[t] * CMF2deg_360_830_Z[t] * CIE_TCS14plus1_360_830[i][t];
        }
        R.Xi[i] = R.K * R.Xi[i];
        R.Yi[i] = R.K * R.Yi[i];
        R.Zi[i] = R.K * R.Zi[i];

        //試験色の基準の光による色度座標の計算
        R.ui[i] = 4 * R.Xi[i] / (R.Xi[i] + 15 * R.Yi[i] + 3 * R.Zi[i]);
        R.vi[i] = 6 * R.Yi[i] / (R.Xi[i] + 15 * R.Yi[i] + 3 * R.Zi[i]);
        /*
                R.k = R.Xi[i] + R.Yi[i] + R.Zi[i];
                R.xi[i] = R.Xi[i] / R.k;
                R.yi[i] = R.Yi[i] / R.k;
                R.zi[i] = R.Zi[i] / R.k;
                R.ui[i] = 4 * R.xi[i] / (-2 * R.xi[i] + 12 * R.yi[i] + 3);
                R.vi[i] = 6 * R.yi[i] / (-2 * R.xi[i] + 12 * R.yi[i] + 3);
        */

        // 6.試験色の色度座標に対する色順応補正
        double c, d; // 光源色の色度座標から求める係数
        K.ud = R.u;
        K.vd = R.v;
        K.c = (4l - K.u - 10l * K.v) / K.v;
        K.d = (1.708 * K.v + 0.404 - 1.481 * K.u) / K.v;
        R.c = (4l - R.u - 10l * R.v) / R.v;
        R.d = (1.708 * R.v + 0.404 - 1.481 * R.u) / R.v;
        K.ci[i] = (4l - K.ui[i] - 10l * K.vi[i]) / K.vi[i];
        K.di[i] = (1.708 * K.vi[i] + 0.404 - 1.481 * K.ui[i]) / K.vi[i];

        //ここの式が間違っている文献があったので注意
        K.udi[i] = (10.872 + 0.404 * R.c / K.c * K.ci[i] - 4l * R.d / K.d * K.di[i]) /
                   (16.518 + 1.481 * R.c / K.c * K.ci[i] - R.d / K.d * K.di[i]);
        K.vdi[i] = 5.52 /
                   (16.518 + 1.481 * R.c / K.c * K.ci[i] - R.d / K.d * K.di[i]);

        // 7.1964均等色空間への変換
        R.w_CIE1964[i] = 25l * pow(R.Yi[i], 1.0l / 3.0l) - 17l;
        R.u_CIE1964[i] = 13l * R.w_CIE1964[i] * (R.ui[i] - R.u);
        R.v_CIE1964[i] = 13l * R.w_CIE1964[i] * (R.vi[i] - R.v);

        K.w_CIE1964[i] = 25l * pow(K.Yi[i], 1.0l / 3.0l) - 17l;
        K.u_CIE1964[i] = 13l * K.w_CIE1964[i] * (K.udi[i] - K.ud);
        K.v_CIE1964[i] = 13l * K.w_CIE1964[i] * (K.vdi[i] - K.vd);

        // 8.演色による色ずれの計算
        K.ri[i] = 100 - 4.6 * sqrt(pow((R.u_CIE1964[i] - K.u_CIE1964[i]), 2) + pow((R.v_CIE1964[i] - K.v_CIE1964[i]), 2) + pow((R.w_CIE1964[i] - K.w_CIE1964[i]), 2));
        if (K.ri[i] <= 0)
        {
            K.ri[i] = 0;
        }
    }
    // 9.演色評価数の計算
    //規格としては小数点にか丸めする必要があるが、丸め処理は表示の時にすることにして、ここではdoubleで値を保持する
    K.ra = 0;
    for (int i = 0; i < 8; i++)
    {
        K.ra = K.ra + K.ri[i];
    }
    K.ra = K.ra / 8;
}

// センサーのPIXステップのスペクトル(分光分布)から5nm STEPのスペクトルデータに変換するメソッド
void conv_raw_to_step5(uint32_t *u32data, double *ddata)
{
    uint16_t t1 = 0;
    uint16_t t2 = 0;

    for (int step5 = 0; step5 < 95; step5++) // 360nm to 830nm step 5nm : 95step
    {
        for (int i = 0; i < PIXCEL_NUM; i++)
        {
            if ((360 + (step5 * 5)) < c12880ma.d_lamdas[i])
            {
                t1 = i - 1;
                t2 = i;
                break;
            }
        }
        ddata[step5] = (double)u32data[t1] +
                       ((360 + (step5 * 5)) - c12880ma.d_lamdas[t1]) *
                           (((double)u32data[t2] - (double)u32data[t1]) / (c12880ma.d_lamdas[t2] - c12880ma.d_lamdas[t1]));
    }
}

// PIXステップのスペクトル(分光分布)から描画画面のPIXステップのスペクトルデータに変換するメソッド
void conv_raw_to_lcdy(uint32_t *u32data, double *ddata)
{
    uint16_t t1 = 0;
    uint16_t t2 = 0;

    //表示範囲の正規化。スペクトラム表示エリアの表示範囲を測定波長範囲全体とした

    for (int step = 0; step < SPECTRUM_PIXCEL_NUM_X; step++)
    {
        for (int i = 0; i < PIXCEL_NUM; i++)
        {
            if ((c12880ma.d_lamdas[0] + t_per_pix * step) < c12880ma.d_lamdas[i])
            {
                t1 = i - 1;
                t2 = i;
                break;
            }
        }

        ddata[step] = (double)u32data[t1] +
                      ((c12880ma.d_lamdas[0] + t_per_pix * step) - c12880ma.d_lamdas[t1]) *
                          (((double)u32data[t2] - (double)u32data[t1]) / (c12880ma.d_lamdas[t2] - c12880ma.d_lamdas[t1]));
    }
}
void update_max(double *ddata)
{
    databuf_lcdy_max = 0;
    for (int i = 0; i < SPECTRUM_PIXCEL_NUM_X; i++)
    {
        if (ddata[i] > databuf_lcdy_max)
        {
            databuf_lcdy_max = ddata[i];
        }
    }
}

void update_offset(uint16_t averagecount)
{
    double offset_buf = 0;
    delay_time_microsecond = 4;
    M5.Lcd.fillScreen(TFT_BLACK); //結果表示のため、画面全体を塗りつぶし

    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK); //文字色設定と背景色設定
    M5.Lcd.setTextFont(4);                     //中ぐらい
    M5.Lcd.setCursor(10, 50);
    M5.Lcd.printf("OFFSET CAL. START.");

    c12880ma.set_delay_time_microsecond(delay_time_microsecond - 1);

    //オフセット値はピクセル固有なので配列で持つ
    for (int i = 0; i < PIXCEL_NUM; i++)
    {
        offset_arr[i] = 0;
    }
    //オフセットにも若干ばらつきがあるので、平均化処理は入れたほうがいい
    for (int j = 0; j < averagecount; j++)
    {
        c12880ma.read_data(Ref);
        for (int i = 0; i < PIXCEL_NUM; i++)
        {
            offset_arr[i] += Ref.data[i];
        }
    }
    for (int i = 0; i < PIXCEL_NUM; i++)
    {
        offset_arr[i] = offset_arr[i] / averagecount;
    }
    Serial.println("OFFSET");
    for (int i = 0; i < PIXCEL_NUM; i++)
    {
        Serial.println(offset_arr[i]);
    }
    M5.Lcd.setCursor(10, 110);
    M5.Lcd.printf("OFFSET CAL. COMPLETE.");
    M5.Lcd.setCursor(50, 200);
    M5.Lcd.printf("x1            x16      OFFSET");
}

void update_color_meter(uint16_t averagecount)
{

    // AGC
    delay_time_microsecond = 4; // 周期に相当する。蓄積時間と比例関係にある。内部処理としてはdelay_time_microsecond-1[us]をdelayとして使っている
    int offset_data = 0;
    for (int i = 0; i < 10; i++)
    {
        c12880ma.set_delay_time_microsecond(delay_time_microsecond - 1);
        sprintf(s, "AGC : %d ns\r\n", delay_time_microsecond);
        Serial.print(s);

        c12880ma.read_data(Ref);

        Ref.update_max();
        if (Ref.max_value > 1200)
            break;
        delay_time_microsecond *= 2;
    }

    // 測定開始
    for (int i = 0; i < PIXCEL_NUM; i++)
    {
        databuf[i] = 0;
    }
    //AGCだけでいいかと思ったが、低照度の測定でばらつくので、平均化処理は残した
    for (int i = 0; i < averagecount; i++)
    {
        c12880ma.read_data(Ref);
        for (int j = 0; j < PIXCEL_NUM; j++)
        {
            databuf[j] += Ref.data[j];
        }
    }
    sprintf(s, "AVG : %d count\r\n", averagecount);
    Serial.print(s);
    Serial.println("RAW DATA START");
    for (int i = 0; i < PIXCEL_NUM; i++)
    {
        Serial.println(databuf[i]);
    }
    Serial.println("RAW DATA END");

    for (int i = 0; i < PIXCEL_NUM; i++)
    {
        //オフセットの計算部分
        // 1項：基準となる4ns設定のオフセットテーブルoffset_arr
        // 2項：蓄積時間（delay_time_microsecond）に比例してオフセットが増える
        // 3項：蓄積時間が増えると、センサーピクセル順に右肩上がりにオフセットが増える
        offset_data = int(offset_arr[i] + ((double)delay_time_microsecond) * 35.0f / 2048.0f + ((double)delay_time_microsecond) * 10.0f / 2048.0f * (i) / 288.0f);

        //オフセットがばらついたときにマイナスになるのを防止する部分
        if (databuf[i] < offset_data * averagecount)
        {
            databuf[i] = 0;
        }
        else
        {
            //オフセットと感度補正の適用。
            databuf[i] = (databuf[i] - offset_data * averagecount) * 100 / Relative_Sensitivity_288[i];
        }
    }

    //演色評価は5nmステップなので、データ間隔を変換する
    conv_raw_to_step5(databuf, K.data);

    Serial.println("360nm TO 830nm 5nm STEP FIX DATA START");
    for (int i = 0; i < 95; i++)
    {
        Serial.println(K.data[i]);
    }
    Serial.println("360nm TO 830nm 5nm STEP FIX DATA END");

    //測定データをLCD表示間隔に変換する
    conv_raw_to_lcdy(databuf, databuf_lcdy);

    //LCD表示データの正規化のため、MAX値を算出しておく
    update_max(databuf_lcdy);

    M5.Lcd.fillScreen(TFT_BLACK);              //結果表示のため、画面全体を塗りつぶし
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK); //文字色設定と背景色設定
    M5.Lcd.setTextFont(4);                     //中ぐらい
    M5.Lcd.setCursor(10, 50);
    M5.Lcd.printf("UPDATE ....");

    //演色評価数の計算を実行
    calc_CRI();


    //スペクトル、演色評価数等の表示を開始
    update_screen();

//    Ref.color = TFT_BLACK;
//    Trans.color = TFT_WHITE;
    for (int i = 0; i < 15; i++)
    {
        sprintf(s, "%02d : %f\r\n", i + 1, K.ri[i]); //規格上は整数で丸める必要があるが、デバッグのため丸めずに出力する
        Serial.print(s);
    }
    sprintf(s, "Ra : %f\r\n", K.ra); //規格上は整数で丸める必要があるが、デバッグのため丸めずに出力する
    Serial.print(s);

    Serial.println();

    Serial.println("END");
}

//-----------------
void setup()
{
    M5.begin();
    SPIFFS.begin(); // xy色度図の背景xy.pngを表示するために使用している.これを計算で実装するととても重い。
    
    init_screen(); //初期画面の表示。
}

void loop()
{

    // M5Stackのボタン入力に応じた処理
    if (M5.BtnA.wasPressed())
    {
        update_color_meter(AVERAGE_COUNT_A); //平均回数１回。１回でも結構重いので、いつもはこっちを使うといい
    }
    if (M5.BtnB.wasPressed())
    {
        update_color_meter(AVERAGE_COUNT_B); //センサー補正値を決めたいときは、ここを128回などとして測定回数を多くするといい。
    }
    if (M5.BtnC.wasPressed())
    {
        update_offset(AVERAGE_COUNT_OFFSET); //オフセットキャリブレーション
    }

    M5.update();
}
