#include <stdint.h>

#include <FastLED.h>
#include <RTCZero.h>
#include <WiFiNINA.h>

#include "WiFiCredentials.h"

#define LED_PIN 13
#define NUM_LEDS 4

bool led;

CRGB leds[NUM_LEDS];

RTCZero rtc;
const int GMT = -5; // EST

char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASS;
int status = WL_IDLE_STATUS; // the Wifi radio's status

const uint32_t WIFI_REFRESH = 60;
uint32_t seconds_since_wifi_update = WIFI_REFRESH;

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
    led = false;
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, led);
    Serial.println("LED set as output");

    //Set up and disable LED strip
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    for (int i = 0; i < NUM_LEDS; i++)
    {
        leds[i] = CRGB::Black;
    }
    FastLED.show();
    Serial.println("LED strip reset");

    Serial.println("Setup Done");
}

void loop()
{
    //Update RTC from WIFI if WIFI_REFRESH seconds have passed
    if (seconds_since_wifi_update >= WIFI_REFRESH)
    {
        if (connectedToWifi())
        {
            unsigned long epoch;

            int numberOfTries = 0, maxTries = 6;
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
                Serial.println();
            }

            seconds_since_wifi_update = 0;
        }
        else
        {
            Serial.print("Attempting to connect to WPA SSID: ");
            Serial.println(ssid);
            // Connect to WPA/WPA2 network:
            status = WiFi.begin(ssid, pass);

            // wait 10 seconds for connection:
            delay(10000);
        }
    }

    //Printout current date/time
    printDate();
    Serial.print(" ");
    printTime();
    Serial.println();

    //Update LED Strip
    leds[0] = CRGB::Red;
    leds[1] = CRGB::Green;
    leds[2] = CRGB::Blue;
    leds[3] = CRGB::White;
    FastLED.show();

    //Blink LED
    led = !led;
    digitalWrite(LED_BUILTIN, led);

    //Wait a second
    delay(1000);
    seconds_since_wifi_update++;
}

//RTC Helper Functions

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
    Serial.print(rtc.getDay());
    Serial.print("/");
    Serial.print(rtc.getMonth());
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
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
}

//General Helper Functions
void print2digits(int number)
{
    if (number < 10)
    {
        Serial.print("0");
    }
    Serial.print(number);
}