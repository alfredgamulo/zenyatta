// Program to exercise the MD_MAX72XX library
//
// Uses most of the functions in the library
#include <Arduino.h>
#include <MD_MAX72xx.h>
#include <WiFi.h>
#include "time.h"
#include "credentials.h"

// Turn on debug statements to the serial output
#define DEBUG 1

#if DEBUG
#define PRINT(s, x)     \
  {                     \
    Serial.print(F(s)); \
    Serial.print(x);    \
  }
#define PRINTS(x) Serial.print(F(x))
#define PRINTD(x) Serial.println(x, DEC)

#else
#define PRINT(s, x)
#define PRINTS(x)
#define PRINTD(x)

#endif

// Define the number of devices we have in the chain and the hardware interface
// NOTE: These pin numbers will probably not work with your hardware and may
// need to be adapted
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 12

#define CLK_PIN 4  // or SCK
#define DATA_PIN 6 // or MOSI
#define CS_PIN 7   // or SS

// SPI hardware interface
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
// Specific SPI hardware interface
// MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, SPI1, CS_PIN, MAX_DEVICES);
// Arbitrary pins
// MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// We always wait a bit between updates of the display
#define DELAYTIME 100 // in milliseconds
#define ROW_SIZE 8    // MAX72XX modules are 8x8 LED matrices

// Declare scrollText and bounce functions
void scrollText(const char *p);
const char *getTimeString();

void scrollText(const char *p)
{
  uint8_t charWidth;
  uint8_t cBuf[8]; // this should be ok for all built-in fonts

  PRINTS("\nScrolling text");

  while (*p != '\0')
  {
    charWidth = mx.getChar(*p++, sizeof(cBuf) / sizeof(cBuf[0]), cBuf);

    for (uint8_t i = 0; i <= charWidth; i++) // allow space between characters
    {
      mx.transform(MD_MAX72XX::TSL);
      mx.setColumn(0, (i < charWidth) ? cBuf[i] : 0);
      delay(DELAYTIME);
    }
  }

  // Add a tab-spaced gap (8 columns) before continuing
  for (uint8_t i = 0; i < 8; i++)
  {
    mx.transform(MD_MAX72XX::TSL);
    mx.setColumn(0, 0);
    delay(DELAYTIME);
  }
}

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;   // Eastern Standard Time is -5 hours. (5 * 3600)
const int daylightOffset_sec = 3600; // 1 hour for Daylight Savings

const char *getTimeString()
{
  static char timeString[10];
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    return "Err:Time";
  }
  // This formats the time as Hour:Minute (e.g., 14:30)
  strftime(timeString, 10, "%H:%M", &timeinfo);
  return timeString;
}

void setup()
{
#if DEBUG
  Serial.begin(57600);
#endif
  PRINTS("\n[MD_MAX72XX Test & Demo]");

  if (!mx.begin())
    PRINTS("\nMD_MAX72XX initialization failed");

  PRINTS("\nConnecting to Wifi...");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) // 20 second timeout
  {
    delay(500);
    PRINTS(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    PRINTS("\nWiFi Connected");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  }
  else
  {
    PRINTS("\nWiFi Connection Failed!");
  }
}

void loop()
{
#if 1
  if (WiFi.status() == WL_CONNECTED)
  {
    scrollText(getTimeString());
  }
  else
  {
    scrollText("WiFi Error");
  }
#endif
}
