#include <stdint.h>

#include <FastLED.h>
#include <RTCZero.h>
#include <WiFiNINA.h>

#include "WiFiCredentials.h"

#define LED_PIN 13
#define NUM_LEDS 4

//Word Clock
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
#define WC_COLUMNS 12
#define WC_ROWS 10
const uint32_t MILLIS_UPDATE_WC = 100;
uint64_t millis_wc_update = 0; //Time in milliseconds from when the led strip was last updated
CRGB leds[WC_ROWS * WC_COLUMNS];

//Builtin LED
const uint32_t MILLIS_UPDATE_LED = 500;
uint8_t ledState = LOW;
uint64_t millis_led_update = 0; //Time in milliseconds from when the led was last updated

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
    while (!Serial)
        ;

    //Start RTC
    rtc.begin();
    Serial.println("RTC started");

    //Set up LED as output
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.println("LED set as output");

    //Set up and disable LED strip
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, WC_ROWS * WC_COLUMNS);
    for (uint8_t i = 0; i < WC_ROWS * WC_COLUMNS; i++)
    {
        leds[i] = CRGB::Black;
    }
    FastLED.show();
    Serial.println("LED strip reset");

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
        leds[0] = CRGB::Red;
        leds[1] = CRGB::Green;
        leds[2] = CRGB::Blue;
        leds[3] = CRGB::White;
        FastLED.show();
        millis_wc_update = millis_loop_start;
    }

    //Blink LED
    if ((millis_loop_start - millis_led_update) >= MILLIS_UPDATE_LED)
    {
        if (ledState == LOW)
        {
            ledState = HIGH;
        }
        else
        {
            ledState = LOW;
        }
        digitalWrite(LED_BUILTIN, ledState);
        millis_led_update = millis_loop_start;
    }
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