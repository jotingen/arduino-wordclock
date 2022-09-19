#include <Arduino.h>

#include <FastLED.h>
#include <RTCZero.h>
#include <WiFiNINA.h>

#include "WiFiCredentials.h"
#define SENSOR_PIN A0
#define LED_PIN 13
#define NUM_LEDS 1000 // Use large number to avoid flickering with dithering

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

// Word Clock
//  LEDs:
//  119 118 ... 109 108
//  96  97  ... 106 107
//  95  94  ... 85  84
//  72  73  ... 82  83
//  71  70  ... 61  60
//  48  49  ... 58  59
//  47  46  ... 37  36
//  24  25  ... 34  35
//  23  22  ... 13  12
//  11  10  ... 1   0
//
//  Index:
//  [0,0] -> [11,0]
//  [0,1] -> [11,1]
//  [0,2] -> [11,2]
//  [0,3] -> [11,3]
//  [0,4] -> [11,4]
//  [0,5] -> [11,5]
//  [0,6] -> [11,6]
//  [0,7] -> [11,7]
//  [0,8] -> [11,8]
//  [0,9] -> [11,9]
//
//  Layout:
// I T T I S I M H A L F E
// A Q U A R T E R N T E N
// T W E N T Y D F I V E D
// P A S T A T O T E O N E
// T W E L V E T I M T W O
// A T H R E E E N F O U R
// F I V E S I X D N I N E
// S E V E N D A E I G H T
// T E N T E L E V E N E T
// I M O ' C L O C K E A N

// IT IS [HALF,QUARTER,TEN,TWENTY,FIVE] [PAST,TO] [ONE,TWELVE,TWO,THREE,FOUR,FIVE,SIX,NINE,SEVEN,EIGHT,TEN,ELEVEN] O'CLOCK
#define WC_X 12
#define WC_Y 10
const uint32_t MILLIS_UPDATE_WC = 100;
uint64_t millis_wc_update = 0; // Time in milliseconds from when the led strip was last updated
CRGB leds[NUM_LEDS];
uint32_t ledNdx = 0;
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

bool wordMask[22];
enum word_t
{
    WC_IT,
    WC_IS,
    WC_A,
    WC_HALF,
    WC_QUARTER,
    WC_TEN,
    WC_TWENTY,
    WC_FIVE,
    WC_PAST,
    WC_TO,
    WC_HOUR_ONE,
    WC_HOUR_TWO,
    WC_HOUR_THREE,
    WC_HOUR_FOUR,
    WC_HOUR_FIVE,
    WC_HOUR_SIX,
    WC_HOUR_NINE,
    WC_HOUR_SEVEN,
    WC_HOUR_EIGHT,
    WC_HOUR_TEN,
    WC_HOUR_ELEVEN,
    WC_HOUR_TWELVE,
    WC_OCLOCK
};

// LED Strip

// RTC
RTCZero rtc;
const int8_t GMT = -5; // EST
uint8_t rtc_set = 0;
uint64_t millis_rtc_update = 0; // Time in milliseconds when RTC was updated

// WIFI
char ssid[] = WIFI_SSID;                            // Set SSID from WiFiCredentials.h
char pass[] = WIFI_PASS;                            // Set SSID from WiFiCredentials.h
const uint32_t MILLIS_WIFI_REFRESH = 3600000;       // Time in milliseconds to refresh epoch from wifi
const uint32_t MILLIS_WIFI_CONNECTION_WAIT = 10000; // Time in milliseconds to wait after starting wifi connection
uint64_t millis_wifi_start_connection = 0;          // Time in milliseconds from when WiFi connection attempt started

// Printouts
const uint32_t MILLIS_PRINTOUT_TIME = 60000; // Time in milliseconds between time print out
uint64_t millis_time_printout = 0;           // Time in milliseconds from when the time was last printed out


void updateWC();
void setWCWord(word_t word);
void setRTCFromWiFi();
uint32_t isDST();
uint32_t dayOfWeek();
void printTime();
void printDate();
bool connectedToWifi();
void connectToWiFi();
void print2digits(uint8_t number);

void setup() {
    // Open serial communications and wait for port to open:
    Serial.begin(115200);
    // while (!Serial)
    //     ;

    // Start RTC
    rtc.begin();
    Serial.println("RTC started");

    // Set up and disable LED strip
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 1000);
    FastLED.setDither(1);
    for (uint8_t i = 0; i < WC_Y * WC_X; i++)
    {
        leds[i] = CRGB::Black;
    }
    FastLED.show();
    Serial.println("LED strip reset");

    // Initialize LED map
    CRGBArray<2> wc_led_it;

    connectToWiFi();

    Serial.println("Setup Done");
}

void loop() {
    uint64_t millis_loop_start = millis();

    // Printout current date/time
    if ((millis_loop_start - millis_time_printout) >= MILLIS_PRINTOUT_TIME)
    {
        printDate();
        Serial.print(" ");
        printTime();
        Serial.println();
        millis_time_printout = millis_loop_start;
    }

    // Update RTC from WIFI if
    //  (    RTC has not been set yet
    //    OR WIFI_REFRESH milliseconds have passed since last time )
    //  AND WIFI_CONNECTION_WAIT milliseconds have passed since last connection attempt
    if ((!rtc_set || (millis_loop_start - millis_rtc_update) >= MILLIS_WIFI_REFRESH) && (millis_loop_start - millis_wifi_start_connection) >= MILLIS_WIFI_CONNECTION_WAIT)
    {
        setRTCFromWiFi();
    }

    // Update Word Clock
    if ((millis_loop_start - millis_wc_update) >= MILLIS_UPDATE_WC)
    {
        // Get brightness from potentiometer
        uint8_t min_brightness = 10;
        uint8_t brightness = (analogRead(SENSOR_PIN) * (255 - min_brightness)) / 1024 + min_brightness;
        FastLED.setBrightness(brightness);

        // Update background
        //  Rainbow walk
        for (uint16_t y = 0; y < WC_Y; y++)
        {
            for (uint16_t x = 0; x < WC_X; x++)
            {
                // Turn led index into radian
                float ledNdx_rad = ((float)(ledNdx % 1024)) / 1024 * 2 * 3.14;
                // Generate offset for grid
                uint32_t x_offset = x * 8 + WC_X * 32;
                uint32_t y_offset = y * 8 + WC_Y * 32;
                // Rotate grid
                float x_rot = ((float)x_offset) * cos(ledNdx_rad) - ((float)y_offset) * sin(ledNdx_rad);
                float y_rot = ((float)y_offset) * cos(ledNdx_rad) + ((float)x_offset) * sin(ledNdx_rad);
                // Convert back to int
                uint32_t x_rot_int = ((uint32_t)x_rot);
                uint32_t y_rot_int = ((uint32_t)y_rot);
                // Update LED array
                leds[ledMap[y][x]].setHue(inoise16(x_rot_int, y_rot_int));
                leds[ledMap[y][x]].fadeToBlackBy(192);
            }
        }
        ledNdx += 1;

        // Update Word Clock
        updateWC();

        FastLED.show();

        millis_wc_update = millis_loop_start;

        FastLED.delay(30);
    }
}

// Word Clock

// Update the LED mask based on time of day
void updateWC()
{
    // IT
    setWCWord(WC_IT);

    // IS
    setWCWord(WC_IS);

    switch (rtc.getMinutes())
    {
    case 0 ... 4:
        // O'CLOCK
        setWCWord(WC_OCLOCK);
        break;
    case 5 ... 9: // Five Past
        setWCWord(WC_FIVE);
        setWCWord(WC_PAST);
        break;
    case 10 ... 14: // Ten Past
        setWCWord(WC_TEN);
        setWCWord(WC_PAST);
        break;
    case 15 ... 19: // Quarter Past
        setWCWord(WC_A);
        setWCWord(WC_QUARTER);
        setWCWord(WC_PAST);
        break;
    case 20 ... 24: // Twenty Past
        setWCWord(WC_TWENTY);
        setWCWord(WC_PAST);
        break;
    case 25 ... 29: // Twenty Five Past
        setWCWord(WC_TWENTY);
        setWCWord(WC_FIVE);
        setWCWord(WC_PAST);
        break;
    case 30 ... 34: // Half Past
        setWCWord(WC_HALF);
        setWCWord(WC_PAST);
        break;
    case 35 ... 39: // Twenty Five To
        setWCWord(WC_TWENTY);
        setWCWord(WC_FIVE);
        setWCWord(WC_TO);
        break;
    case 40 ... 44: // Twenty To
        setWCWord(WC_TWENTY);
        setWCWord(WC_TO);
        break;
    case 45 ... 49: // Quarter To
        setWCWord(WC_A);
        setWCWord(WC_QUARTER);
        setWCWord(WC_TO);
        break;
    case 50 ... 54: // Ten To
        setWCWord(WC_TEN);
        setWCWord(WC_TO);
        break;
    case 55 ... 59: // Five To
        setWCWord(WC_FIVE);
        setWCWord(WC_TO);
        break;
    }
    // HOURS
    switch ((((rtc.getMinutes() >= 35) ? 1 : 0) + rtc.getHours() + isDST()) % 12)
    {
    case 0:
        setWCWord(WC_HOUR_TWELVE);
        break;
    case 1:
        setWCWord(WC_HOUR_ONE);
        break;
    case 2:
        setWCWord(WC_HOUR_TWO);
        break;
    case 3:
        setWCWord(WC_HOUR_THREE);
        break;
    case 4:
        setWCWord(WC_HOUR_FOUR);
        break;
    case 5:
        setWCWord(WC_HOUR_FIVE);
        break;
    case 6:
        setWCWord(WC_HOUR_SIX);
        break;
    case 7:
        setWCWord(WC_HOUR_SEVEN);
        break;
    case 8:
        setWCWord(WC_HOUR_EIGHT);
        break;
    case 9:
        setWCWord(WC_HOUR_NINE);
        break;
    case 10:
        setWCWord(WC_HOUR_TEN);
        break;
    case 11:
        setWCWord(WC_HOUR_ELEVEN);
        break;
    }
}

void setWCWord(word_t word)
{

    switch (word)
    {
    case WC_IT:
        leds[ledMap[0][0]].setRGB(255, 255, 255);
        leds[ledMap[0][1]].setRGB(255, 255, 255);
        break;
    case WC_IS:
        leds[ledMap[0][3]].setRGB(255, 255, 255);
        leds[ledMap[0][4]].setRGB(255, 255, 255);
        break;
    case WC_A:
        leds[ledMap[0][8]].setRGB(255, 255, 255);
        break;
    case WC_FIVE:
        leds[ledMap[2][7]].setRGB(255, 255, 255);
        leds[ledMap[2][8]].setRGB(255, 255, 255);
        leds[ledMap[2][9]].setRGB(255, 255, 255);
        leds[ledMap[2][10]].setRGB(255, 255, 255);
        break;
    case WC_TEN:
        leds[ledMap[1][9]].setRGB(255, 255, 255);
        leds[ledMap[1][10]].setRGB(255, 255, 255);
        leds[ledMap[1][11]].setRGB(255, 255, 255);
        break;
    case WC_QUARTER:
        leds[ledMap[1][1]].setRGB(255, 255, 255);
        leds[ledMap[1][2]].setRGB(255, 255, 255);
        leds[ledMap[1][3]].setRGB(255, 255, 255);
        leds[ledMap[1][4]].setRGB(255, 255, 255);
        leds[ledMap[1][5]].setRGB(255, 255, 255);
        leds[ledMap[1][6]].setRGB(255, 255, 255);
        leds[ledMap[1][7]].setRGB(255, 255, 255);
        break;
    case WC_TWENTY:
        leds[ledMap[2][0]].setRGB(255, 255, 255);
        leds[ledMap[2][1]].setRGB(255, 255, 255);
        leds[ledMap[2][2]].setRGB(255, 255, 255);
        leds[ledMap[2][3]].setRGB(255, 255, 255);
        leds[ledMap[2][4]].setRGB(255, 255, 255);
        leds[ledMap[2][5]].setRGB(255, 255, 255);
        break;
    case WC_HALF:
        leds[ledMap[0][7]].setRGB(255, 255, 255);
        leds[ledMap[0][8]].setRGB(255, 255, 255);
        leds[ledMap[0][9]].setRGB(255, 255, 255);
        leds[ledMap[0][10]].setRGB(255, 255, 255);
        break;
    case WC_TO:
        leds[ledMap[3][5]].setRGB(255, 255, 255);
        leds[ledMap[3][6]].setRGB(255, 255, 255);
        break;
    case WC_PAST:
        leds[ledMap[3][0]].setRGB(255, 255, 255);
        leds[ledMap[3][1]].setRGB(255, 255, 255);
        leds[ledMap[3][2]].setRGB(255, 255, 255);
        leds[ledMap[3][3]].setRGB(255, 255, 255);
        break;
    case WC_HOUR_ONE:
        leds[ledMap[3][9]].setRGB(255, 255, 255);
        leds[ledMap[3][10]].setRGB(255, 255, 255);
        leds[ledMap[3][11]].setRGB(255, 255, 255);
        break;
    case WC_HOUR_TWO:
        leds[ledMap[4][9]].setRGB(255, 255, 255);
        leds[ledMap[4][10]].setRGB(255, 255, 255);
        leds[ledMap[4][11]].setRGB(255, 255, 255);
        break;
    case WC_HOUR_THREE:
        leds[ledMap[5][1]].setRGB(255, 255, 255);
        leds[ledMap[5][2]].setRGB(255, 255, 255);
        leds[ledMap[5][3]].setRGB(255, 255, 255);
        leds[ledMap[5][4]].setRGB(255, 255, 255);
        leds[ledMap[5][5]].setRGB(255, 255, 255);
        break;
    case WC_HOUR_FOUR:
        leds[ledMap[5][8]].setRGB(255, 255, 255);
        leds[ledMap[5][9]].setRGB(255, 255, 255);
        leds[ledMap[5][10]].setRGB(255, 255, 255);
        leds[ledMap[5][11]].setRGB(255, 255, 255);
        break;
    case WC_HOUR_FIVE:
        leds[ledMap[6][0]].setRGB(255, 255, 255);
        leds[ledMap[6][1]].setRGB(255, 255, 255);
        leds[ledMap[6][2]].setRGB(255, 255, 255);
        leds[ledMap[6][3]].setRGB(255, 255, 255);
        break;
    case WC_HOUR_SIX:
        leds[ledMap[6][4]].setRGB(255, 255, 255);
        leds[ledMap[6][5]].setRGB(255, 255, 255);
        leds[ledMap[6][6]].setRGB(255, 255, 255);
        break;
    case WC_HOUR_SEVEN:
        leds[ledMap[7][0]].setRGB(255, 255, 255);
        leds[ledMap[7][1]].setRGB(255, 255, 255);
        leds[ledMap[7][2]].setRGB(255, 255, 255);
        leds[ledMap[7][3]].setRGB(255, 255, 255);
        leds[ledMap[7][4]].setRGB(255, 255, 255);
        break;
    case WC_HOUR_EIGHT:
        leds[ledMap[7][7]].setRGB(255, 255, 255);
        leds[ledMap[7][8]].setRGB(255, 255, 255);
        leds[ledMap[7][9]].setRGB(255, 255, 255);
        leds[ledMap[7][10]].setRGB(255, 255, 255);
        leds[ledMap[7][11]].setRGB(255, 255, 255);
        break;
    case WC_HOUR_NINE:
        leds[ledMap[6][8]].setRGB(255, 255, 255);
        leds[ledMap[6][9]].setRGB(255, 255, 255);
        leds[ledMap[6][10]].setRGB(255, 255, 255);
        leds[ledMap[6][11]].setRGB(255, 255, 255);
        break;
    case WC_HOUR_TEN:
        leds[ledMap[8][0]].setRGB(255, 255, 255);
        leds[ledMap[8][1]].setRGB(255, 255, 255);
        leds[ledMap[8][2]].setRGB(255, 255, 255);
        break;
    case WC_HOUR_ELEVEN:
        leds[ledMap[8][4]].setRGB(255, 255, 255);
        leds[ledMap[8][5]].setRGB(255, 255, 255);
        leds[ledMap[8][6]].setRGB(255, 255, 255);
        leds[ledMap[8][7]].setRGB(255, 255, 255);
        leds[ledMap[8][8]].setRGB(255, 255, 255);
        leds[ledMap[8][9]].setRGB(255, 255, 255);
        break;
    case WC_HOUR_TWELVE:
        leds[ledMap[4][0]].setRGB(255, 255, 255);
        leds[ledMap[4][1]].setRGB(255, 255, 255);
        leds[ledMap[4][2]].setRGB(255, 255, 255);
        leds[ledMap[4][3]].setRGB(255, 255, 255);
        leds[ledMap[4][4]].setRGB(255, 255, 255);
        leds[ledMap[4][5]].setRGB(255, 255, 255);
        break;
    case WC_OCLOCK:
        leds[ledMap[9][2]].setRGB(255, 255, 255);
        leds[ledMap[9][3]].setRGB(255, 255, 255);
        leds[ledMap[9][4]].setRGB(255, 255, 255);
        leds[ledMap[9][5]].setRGB(255, 255, 255);
        leds[ledMap[9][6]].setRGB(255, 255, 255);
        leds[ledMap[9][7]].setRGB(255, 255, 255);
        leds[ledMap[9][8]].setRGB(255, 255, 255);
        break;
    }
}

// RTC Helper Functions

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
            rtc.setEpoch(epoch + GMT * 3600);
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

uint32_t isDST()
{

    // Second Sunday in March at 2:00	First Sunday in November at 2:00

    // Not daylight savings if before Mar or after Nov
    if (rtc.getMonth() < 3 || rtc.getMonth() > 11)
    {
        return 0;
    }

    // Daylight savings if after Mar and before Nov
    if (rtc.getMonth() > 3 && rtc.getMonth() < 11)
    {
        return 1;
    }

    // If after second sunday in march
    // Second sunday if day >7 and <=14 and sunday
    if (rtc.getMonth() == 3)
    {
        // Past second week
        if (rtc.getDay() > 14)
        {
            return 1;
        }
        // Second week
        if (rtc.getDay() == 14)
        {
            // Is Sunday
            if (dayOfWeek() == 0)
            {
                // Past 2:00
                if (rtc.getHours() >= 2)
                {
                    return 1;
                }
            }
            // Past Sunday
            if (dayOfWeek() <= 6)
            {
                return 1;
            }
        }
        if (rtc.getDay() == 13)
        {
            // Is Sunday
            if (dayOfWeek() == 0)
            {
                // Past 2:00
                if (rtc.getHours() >= 2)
                {
                    return 1;
                }
            }
            // Past Sunday
            if (dayOfWeek() <= 5)
            {
                return 1;
            }
        }
        if (rtc.getDay() == 12)
        {
            // Is Sunday
            if (dayOfWeek() == 0)
            {
                // Past 2:00
                if (rtc.getHours() >= 2)
                {
                    return 1;
                }
            }
            // Past Sunday
            if (dayOfWeek() <= 4)
            {
                return 1;
            }
        }
        if (rtc.getDay() == 11)
        {
            // Is Sunday
            if (dayOfWeek() == 0)
            {
                // Past 2:00
                if (rtc.getHours() >= 2)
                {
                    return 1;
                }
            }
            // Past Sunday
            if (dayOfWeek() <= 3)
            {
                return 1;
            }
        }
        if (rtc.getDay() == 10)
        {
            // Is Sunday
            if (dayOfWeek() == 0)
            {
                // Past 2:00
                if (rtc.getHours() >= 2)
                {
                    return 1;
                }
            }
            // Past Sunday
            if (dayOfWeek() <= 2)
            {
                return 1;
            }
        }
        if (rtc.getDay() == 9)
        {
            // Is Sunday
            if (dayOfWeek() == 0)
            {
                // Past 2:00
                if (rtc.getHours() >= 2)
                {
                    return 1;
                }
            }
            // Past Sunday
            if (dayOfWeek() <= 1)
            {
                return 1;
            }
        }
        if (rtc.getDay() == 8)
        {
            // Is Sunday
            if (dayOfWeek() == 0)
            {
                // Past 2:00
                if (rtc.getHours() >= 2)
                {
                    return 1;
                }
            }
        }
        return 0;
    }

    // If before first sunday in nov
    // Second sunday if day <= 7 and sunday
    if (rtc.getMonth() == 11)
    {
        // First week
        if (rtc.getDay() == 7)
        {
            // Is Sunday
            if (dayOfWeek() == 0)
            {
                // Before 2:00
                if (rtc.getHours() < 2)
                {
                    return 1;
                }
            }
            // Before Sunday
            if (dayOfWeek() < 1)
            {
                return 1;
            }
        }
        if (rtc.getDay() == 6)
        {
            // Is Sunday
            if (dayOfWeek() == 0)
            {
                // Before 2:00
                if (rtc.getHours() < 2)
                {
                    return 1;
                }
            }
            // Before Sunday
            if (dayOfWeek() >= 6)
            {
                return 1;
            }
        }
        if (rtc.getDay() == 5)
        {
            // Is Sunday
            if (dayOfWeek() == 0)
            {
                // Before 2:00
                if (rtc.getHours() < 2)
                {
                    return 1;
                }
            }
            // Before Sunday
            if (dayOfWeek() >= 5)
            {
                return 1;
            }
        }
        if (rtc.getDay() == 4)
        {
            // Is Sunday
            if (dayOfWeek() == 0)
            {
                // Before 2:00
                if (rtc.getHours() < 2)
                {
                    return 1;
                }
            }
            // Before Sunday
            if (dayOfWeek() >= 4)
            {
                return 1;
            }
        }
        if (rtc.getDay() == 3)
        {
            // Is Sunday
            if (dayOfWeek() == 0)
            {
                // Before 2:00
                if (rtc.getHours() < 2)
                {
                    return 1;
                }
            }
            // Before Sunday
            if (dayOfWeek() >= 3)
            {
                return 1;
            }
        }
        if (rtc.getDay() == 2)
        {
            // Is Sunday
            if (dayOfWeek() == 0)
            {
                // Before 2:00
                if (rtc.getHours() < 2)
                {
                    return 1;
                }
            }
            // Before Sunday
            if (dayOfWeek() >= 2)
            {
                return 1;
            }
        }
        if (rtc.getDay() == 1)
        {
            // Is Sunday
            if (dayOfWeek() == 0)
            {
                // Before 2:00
                if (rtc.getHours() < 2)
                {
                    return 1;
                }
            }
            // Before Sunday
            if (dayOfWeek() >= 1)
            {
                return 1;
            }
        }
        return 0;
    }
    return 0;
}

uint32_t dayOfWeek()
{

    uint32_t seconds_per_day = 86400;
    uint32_t epoch_start_day_offset = 4; // Adjust for start day being thursday
    return ((rtc.getEpoch() / seconds_per_day) + epoch_start_day_offset) % 7;
}

void printTime()
{
    print2digits(rtc.getHours());
    Serial.print(":");
    print2digits(rtc.getMinutes());
    Serial.print(":");
    print2digits(rtc.getSeconds());
    Serial.println();
}

void printDate()
{
    Serial.print(isDST());
    Serial.print(" ");
    Serial.print(dayOfWeek());
    Serial.print(" ");
    Serial.print(rtc.getEpoch());
    Serial.print(" ");
    Serial.print(rtc.getMonth());
    Serial.print("/");
    Serial.print(rtc.getDay());
    Serial.print("/");
    Serial.print(rtc.getYear());
    Serial.print(" ");
}

// WiFi Helper Functions

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

// General Helper Functions
void print2digits(uint8_t number)
{
    if (number < 10)
    {
        Serial.print("0");
    }
    Serial.print(number);
}