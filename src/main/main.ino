#include <stdint.h>

#include <FastLED.h>
#include <RTCZero.h>
#include <WiFiNINA.h>

#include "WiFiCredentials.h"
#define SENSOR_PIN A0   
#define LED_PIN 13
#define NUM_LEDS 4

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

//Word Clock
// LEDs:
// 119 118 ... 109 108
// 96  97  ... 106 107
// 95  94  ... 85  84
// 72  73  ... 82  83
// 71  70  ... 61  60
// 48  49  ... 58  59
// 47  46  ... 37  36
// 24  25  ... 34  35
// 23  22  ... 13  12
// 11  10  ... 1   0
//
// Index:
// [0,0] -> [11,0]
// [0,1] -> [11,1]
// [0,2] -> [11,2]
// [0,3] -> [11,3]
// [0,4] -> [11,4]
// [0,5] -> [11,5]
// [0,6] -> [11,6]
// [0,7] -> [11,7]
// [0,8] -> [11,8]
// [0,9] -> [11,9]
//
// Layout:
//I T T I S I M H A L F E
//A Q U A R T E R N T E N
//T W E N T Y D F I V E D
//P A S T A T O T E O N E
//T W E L V E T I M T W O
//A T H R E E E N F O U R
//F I V E S I X D N I N E
//S E V E N D A E I G H T
//T E N T E L E V E N E T
//I M O ' C L O C K E A N

//IT IS [HALF,QUARTER,TEN,TWENTY,FIVE] [PAST,TO] [ONE,TWELVE,TWO,THREE,FOUR,FIVE,SIX,NINE,SEVEN,EIGHT,TEN,ELEVEN] O'CLOCK
#define WC_X 12
#define WC_Y 10
const uint32_t MILLIS_UPDATE_WC = 10;
uint64_t millis_wc_update = 0; //Time in milliseconds from when the led strip was last updated
CRGB leds[WC_Y * WC_X];
uint8_t ledNdx = 0;
uint8_t rgbw = 0;

uint8_t ledMap[WC_Y][WC_X] = {
    {119, 118, 117, 116, 115, 114, 113, 112, 111, 110, 109, 108},
    {96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107},
    {95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84},
    {72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83},
    {71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60},
    {48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59},
    {47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36},
    {24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35},
    {23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}};

uint8_t ledNoise[WC_Y][WC_X];

//LED Strip

//RTC
RTCZero rtc;
const int8_t GMT = -4; // EST
uint8_t rtc_set = 0;
uint64_t millis_rtc_update = 0; //Time in milliseconds when RTC was updated

//WIFI
char ssid[] = WIFI_SSID;                            //Set SSID from WiFiCredentials.h
char pass[] = WIFI_PASS;                            //Set SSID from WiFiCredentials.h
const uint32_t MILLIS_WIFI_REFRESH = 3600000;       //Time in milliseconds to refresh epoch from wifi
const uint32_t MILLIS_WIFI_CONNECTION_WAIT = 10000; //Time in milliseconds to wait after starting wifi connection
uint64_t millis_wifi_start_connection = 0;          //Time in milliseconds from when WiFi connection attempt started

//Printouts
const uint32_t MILLIS_PRINTOUT_TIME = 60000; //Time in milliseconds between time print out
uint64_t millis_time_printout = 0;           //Time in milliseconds from when the time was last printed out

void setup()
{
    // Open serial communications and wait for port to open:
    Serial.begin(115200);
    //while (!Serial)
    //    ;

    //Start RTC
    rtc.begin();
    Serial.println("RTC started");

    //Set up and disable LED strip
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, WC_Y * WC_X);
    for (uint8_t i = 0; i < WC_Y * WC_X; i++)
    {
        leds[i] = CRGB::Black;
    }
    FastLED.show();
    Serial.println("LED strip reset");

    //Iinitialize LED map

    connectToWiFi();

    Serial.println("Setup Done");
}

void loop()
{
    uint64_t millis_loop_start = millis();

    //Printout current date/time
    if ((millis_loop_start - millis_time_printout) >= MILLIS_PRINTOUT_TIME)
    {
        printDate();
        Serial.print(" ");
        printTime();
        Serial.println();
        millis_time_printout = millis_loop_start;
    }

    //Update RTC from WIFI if
    // (    RTC has not been set yet
    //   OR WIFI_REFRESH milliseconds have passed since last time )
    // AND WIFI_CONNECTION_WAIT milliseconds have passed since last connection attempt
    if ((!rtc_set || (millis_loop_start - millis_rtc_update) >= MILLIS_WIFI_REFRESH) && (millis_loop_start - millis_wifi_start_connection) >= MILLIS_WIFI_CONNECTION_WAIT)
    {
        setRTCFromWiFi();
    }

    //Update Word Clock
    if ((millis_loop_start - millis_wc_update) >= MILLIS_UPDATE_WC)
    {
        // Get brightness from potentiometer
        uint8_t brightness = analogRead(SENSOR_PIN)/4;

        //Noise
        // Background noise for other LEDs
        for (uint16_t y = 0; y < WC_Y; y++)
        {
            for (uint16_t x = 0; x < WC_X; x++)
            {
                leds[ledMap[y][x]].red = inoise8((x - 128) + ledNdx, (y) + ledNdx);
                leds[ledMap[y][x]].green = inoise8((x) - ledNdx, (y + 128) - ledNdx);
                leds[ledMap[y][x]].blue = inoise8((x) - ledNdx, (y) + ledNdx);
                leds[ledMap[y][x]].fadeToBlackBy(240);
            }
        }
        ledNdx++;

        //IT
        leds[ledMap[0][0]].setRGB(255,255,255).fadeToBlackBy(brightness);
        leds[ledMap[0][1]].setRGB(255,255,255).fadeToBlackBy(brightness);

        //IS
        leds[ledMap[0][3]].setRGB(255,255,255).fadeToBlackBy(brightness);
        leds[ledMap[0][4]].setRGB(255,255,255).fadeToBlackBy(brightness);

        switch(rtc.getMinutes())
        {
            case 0 ... 4: break;
            case 5 ... 9: //Five Past
            leds[ledMap[2][7]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[2][8]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[2][9]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[2][10]].setRGB(255,255,255).fadeToBlackBy(brightness);

            leds[ledMap[3][0]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][1]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][2]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][3]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;
            case 10 ... 14: //Ten Past
            leds[ledMap[1][9]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[1][10]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[1][11]].setRGB(255,255,255).fadeToBlackBy(brightness);

            leds[ledMap[3][0]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][1]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][2]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][3]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;
            case 15 ... 19: //Quarter Past
            leds[ledMap[1][1]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[1][2]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[1][3]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[1][4]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[1][5]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[1][6]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[1][7]].setRGB(255,255,255).fadeToBlackBy(brightness);

            leds[ledMap[3][0]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][1]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][2]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][3]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;
            case 20 ... 29: //Twenty Past
            leds[ledMap[2][0]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[2][1]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[2][2]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[2][3]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[2][4]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[2][5]].setRGB(255,255,255).fadeToBlackBy(brightness);

            leds[ledMap[3][0]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][1]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][2]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][3]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;
            case 30 ... 39: //Half Past
            leds[ledMap[0][7]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[0][8]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[0][9]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[0][10]].setRGB(255,255,255).fadeToBlackBy(brightness);

            leds[ledMap[3][0]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][1]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][2]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][3]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;
            case 40 ... 44: //Twenty To
            leds[ledMap[2][0]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[2][1]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[2][2]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[2][3]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[2][4]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[2][5]].setRGB(255,255,255).fadeToBlackBy(brightness);

            leds[ledMap[3][5]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][6]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;
            case 45 ... 49: //Quarter To
            leds[ledMap[1][1]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[1][2]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[1][3]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[1][4]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[1][5]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[1][6]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[1][7]].setRGB(255,255,255).fadeToBlackBy(brightness);

            leds[ledMap[3][5]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][6]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;
            case 50 ... 54: //Ten To
            leds[ledMap[1][9]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[1][10]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[1][11]].setRGB(255,255,255).fadeToBlackBy(brightness);

            leds[ledMap[3][5]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][6]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;
            case 55 ... 59: //Five To
            leds[ledMap[2][7]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[2][8]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[2][9]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[2][10]].setRGB(255,255,255).fadeToBlackBy(brightness);

            leds[ledMap[3][5]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][6]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;
        }
        //HOURS
        switch ((rtc.getHours() + GMT) % 12)
        {
        case 0:
            leds[ledMap[4][0]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[4][1]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[4][2]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[4][3]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[4][4]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[4][5]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;

        case 1:
            leds[ledMap[3][9]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][10]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[3][11]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;

        case 2:
            leds[ledMap[4][9]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[4][10]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[4][11]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;

        case 3:
            leds[ledMap[5][1]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[5][2]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[5][3]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[5][4]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[5][5]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;

        case 4:
            leds[ledMap[5][8]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[5][9]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[5][10]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[5][11]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;

        case 5:
            leds[ledMap[6][0]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[6][1]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[6][2]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[6][3]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;

        case 6:
            leds[ledMap[6][4]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[6][5]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[6][6]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;

        case 7:
            leds[ledMap[7][0]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[7][1]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[7][2]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[7][3]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[7][4]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;

        case 8:
            leds[ledMap[7][7]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[7][8]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[7][9]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[7][10]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[7][11]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;

        case 9:
            leds[ledMap[6][8]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[6][9]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[6][10]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[6][11]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;

        case 10:
            leds[ledMap[8][0]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[8][1]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[8][2]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;

        case 11:
            leds[ledMap[8][4]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[8][5]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[8][6]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[8][7]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[8][9]].setRGB(255,255,255).fadeToBlackBy(brightness);
            leds[ledMap[8][10]].setRGB(255,255,255).fadeToBlackBy(brightness);
            break;
        }

        //O'CLOCK
        leds[ledMap[9][2]].setRGB(255,255,255).fadeToBlackBy(brightness);
        leds[ledMap[9][3]].setRGB(255,255,255).fadeToBlackBy(brightness);
        leds[ledMap[9][4]].setRGB(255,255,255).fadeToBlackBy(brightness);
        leds[ledMap[9][5]].setRGB(255,255,255).fadeToBlackBy(brightness);
        leds[ledMap[9][6]].setRGB(255,255,255).fadeToBlackBy(brightness);
        leds[ledMap[9][7]].setRGB(255,255,255).fadeToBlackBy(brightness);
        leds[ledMap[9][8]].setRGB(255,255,255).fadeToBlackBy(brightness);

        //Brightness

        //for (uint16_t y = 0; y < WC_Y; y++)
        //{
        //    for (uint16_t x = 0; x < WC_X; x++)
        //    {
        //        leds[ledMap[y][x]].fadeToBlackBy(brightness);
        //    }
        //}

        FastLED.show();
        millis_wc_update = millis_loop_start;
        Serial.println(analogRead(SENSOR_PIN));
    }

}

//Word Clock

//Update the LED mask based on time of day
void updateWCMask()
{
}

//RTC Helper Functions

void setRTCFromWiFi()
{
    if (connectedToWifi())
    {
        uint32_t epoch;

        uint8_t numberOfTries = 0, maxTries = 6;
        do
        {
            epoch = WiFi.getTime();
            numberOfTries++;
        } while ((epoch == 0) && (numberOfTries < maxTries));

        if (numberOfTries == maxTries)
        {
            Serial.print("NTP unreachable!!");
            while (1)
                ;
        }
        else
        {
            Serial.print("Epoch received: ");
            Serial.println(epoch);
            rtc.setEpoch(epoch);
            rtc_set = 1;
            Serial.println();
        }

        millis_rtc_update = millis();
    }
    else
    {
        connectToWiFi();
    }
}

void printTime()
{
    print2digits(rtc.getHours() + GMT);
    Serial.print(":");
    print2digits(rtc.getMinutes());
    Serial.print(":");
    print2digits(rtc.getSeconds());
    Serial.println();
}

void printDate()
{
    Serial.print(rtc.getMonth());
    Serial.print("/");
    Serial.print(rtc.getDay());
    Serial.print("/");
    Serial.print(rtc.getYear());
    Serial.print(" ");
}

//WiFi Helper Functions

bool connectedToWifi()
{
    return WiFi.status() == WL_CONNECTED;
}

void connectToWiFi()
{
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);

    // Connect to WPA/WPA2 network:
    WiFi.begin(ssid, pass);

    millis_wifi_start_connection = millis();
}

//General Helper Functions
void print2digits(uint8_t number)
{
    if (number < 10)
    {
        Serial.print("0");
    }
    Serial.print(number);
}
