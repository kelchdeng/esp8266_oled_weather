/**dengjie
*/

#include <Arduino.h>

//#include <ESPWiFi.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>


#include <ESP8266mDNS.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
// time
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()

//#include "SSD1306Wire.h"
#include "SH1106Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "OpenWeatherMapCurrent.h"
#include "JDForecast.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"

//#include <FastLED.h>

#include <ArduinoOTA.h>


//LED信号线
//#define LED_PIN 1
//30颗LED
//#define NUM_LEDS 30
//CRGB leds[NUM_LEDS];
/***************************
   Begin Settings
 **************************/
#define HOSTNAME "ESP8266-TQ-"
// WIFI
const char* WIFI_SSID = "";
const char* WIFI_PWD = "";

#define TZ              8       // (utc+) TZ in hours
#define DST_MN          0      // use 60mn for summer time in some countries中国没有夏令时，赋0

// Setup
const int UPDATE_INTERVAL_SECS = 20 * 60; // Update every 20 minutes 每20分钟更新

// Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;
#if defined(ESP8266)
const int SDA_PIN = D3;
const int SDC_PIN = D4;
#else
const int SDA_PIN = 5; //D3;
const int SDC_PIN = 4; //D4;
#endif

// https://wx.jdcloud.com/market/datas/26/10610 
//const String OPEN_WEATHER_MAP_APP_ID = "";
// https://wx.jdcloud.com/market/datas/26/10610 
//const String OPEN_WEATHER_MAP_LOCATION_ID = "";

// https://wx.jdcloud.com/market/datas/26/10610 京东和风 appid
const String JD_APP_ID = "";
// https://wx.jdcloud.com/market/datas/26/10610 京东和风 城市
//弃用
//const String JD_LOCATION_ID = "";
//百度 api
const String BAIDU_AK = "";
const String CITY_URL = "http://api.map.baidu.com/location/ip?coor=bd09ll&ak="+BAIDU_AK;

//弃用
String OPEN_WEATHER_MAP_LANGUAGE = "zh_cn";
//最大预报天数 暂时没用
const uint8_t MAX_FORECASTS = 4;

const boolean IS_METRIC = true;

// Adjust according to your language
const String WDAY_NAMES[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const String MONTH_NAMES[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

/***************************
   End Settings
 **************************/
// Initialize the oled display for address 0x3c
// sda-pin=D3 and sdc-pin=D4
// 设置IIC地址及OLED管脚
SH1106Wire     display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
OLEDDisplayUi   ui( &display );
//当前天气对象实例
JDForecastData currentWeather;
//当前天气客户端对象实例
//OpenWeatherMapCurrent currentWeatherClient;
//预报对象实例数组，数组大小是MAX_FORECASTS定义的值
JDForecastData forecasts[MAX_FORECASTS];
//预报客户端对象实例
JDForecast forecastClient;

#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)
time_t now;

// flag changed in the ticker function every 10 minutes 标记更新
bool readyForWeatherUpdate = false;

String lastUpdate = "--";

long timeSinceLastWUpdate = 0;

//declaring prototypes 声明
void configModeCallback (WiFiManager *myWiFiManager);
void drawProgress(OLEDDisplay *display, int percentage, String label);
void updateData(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawBigTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void setReadyForWeatherUpdate();



// Add frames添加框架
// this array keeps function pointers to all frames 所有帧 的数组
// frames are the single views that slide from right to left 从右向左滑入
//切屏数组
FrameCallback frames[] = { drawBigTime, drawDateTime, drawCurrentWeather, drawForecast };
//屏幕 数组大小
int numberOfFrames = 4;

OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;
int h = 0;


void setup() {
  Serial.begin(115200);
  Serial.println("init");

  // initialize dispaly
  display.init();
  display.clear();
  display.display();

  //display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);

  WiFiConfig();

  //  WiFi.begin(WIFI_SSID, WIFI_PWD);
  //
  //  int counter = 0;
  //  while (WiFi.status() != WL_CONNECTED) {
  //    delay(500);
  //    Serial.print(".");
  //    display.clear();
  //    display.drawString(64, 10, "Connecting to WiFi");
  //    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
  //    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
  //    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
  //    display.display();
  //
  //    counter++;
  //  }
  // Get time from network time service. ntp服务设置
  configTime(TZ_SEC, DST_SEC, "ntp1.aliyun.com");

  ui.setTargetFPS(30);

  ui.setActiveSymbol(activeSymbole);
  ui.setInactiveSymbol(inactiveSymbole);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT 方向
  ui.setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar. 第一帧在中间的位置
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used 滚动方式
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  ui.setFrames(frames, numberOfFrames);

  ui.setOverlays(overlays, numberOfOverlays);

  // Inital UI takes care of initalising the display too.
  ui.init();

  updateData(&display);
  
  //FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  //FastLED.setBrightness(20);//亮度

  // OTA设置并启动
  ArduinoOTA.setHostname("ESP8266-OLED-TQ");
  ArduinoOTA.setPassword("12345678");
  ArduinoOTA.begin();

}

void loop() {
  ArduinoOTA.handle();
  if (millis() - timeSinceLastWUpdate > (1000L * UPDATE_INTERVAL_SECS)) {
    setReadyForWeatherUpdate();
    timeSinceLastWUpdate = millis();
  }

  if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) {
    updateData(&display);
  }

  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    delay(remainingTimeBudget);
  }

  //彩色跑马灯
//  for (int i = 0; i < NUM_LEDS; i++) {
//    leds[i] = CHSV(h, 255, 255);//CRGB::Red;
//    FastLED.show();
//    delay(100);
//    //leds[i] = CRGB::Black;//熄灯
//    h = (h + 3) % 255;
//  }

}
//绘制
void drawProgress(OLEDDisplay *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}
//更新
void updateData(OLEDDisplay *display) {
  drawProgress(display, 10, "Updating time...");
  drawProgress(display, 30, "Updating weather...");
  delay(3000);
  drawProgress(display, 50, "Updating forecasts...");
  forecastClient.updateForecastsById(forecasts, &currentWeather, JD_APP_ID, CITY_URL, MAX_FORECASTS);


  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Done...");
  delay(1000);
}


//绘制日期时间
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[16];


  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String date = WDAY_NAMES[timeInfo->tm_wday];
  //年-月-日 星期
  sprintf_P(buff, PSTR("%04d-%02d-%02d %s"),timeInfo->tm_year + 1900,timeInfo->tm_mon + 1,timeInfo->tm_mday, WDAY_NAMES[timeInfo->tm_wday].c_str() );
  display->drawString(64 + x, 5 + y, String(buff));
  display->setFont(ArialMT_Plain_24);

  sprintf_P(buff, PSTR("%02d:%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
  display->drawString(64 + x, 15 + y, String(buff));
  display->setTextAlignment(TEXT_ALIGN_LEFT);

  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, "RH: "+String(currentWeather.hum)+"%");
  
}
//绘制当前天气
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, "AQI: " + currentWeather.aqi + " PM2.5: " + currentWeather.pm25);

  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String temp = String(currentWeather.tmp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(60 + x, 5 + y, temp);

  display->setFont(Meteocons_Plain_36);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(32 + x, 0 + y, currentWeather.iconMeteoCon);
}

//天气预报
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawForecastDetails(display, x, y, 0);
  drawForecastDetails(display, x + 44, y, 1);
  drawForecastDetails(display, x + 88, y, 2);
}
//预报详情
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex) {
  //预报 小时
  String intHou = String(forecasts[dayIndex].intHou) + ":00";
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y, intHou);
  //天气图标
  display->setFont(Meteocons_Plain_21);
//  display->drawString(x + 20, y + 12, "N");
  display->drawString(x + 20, y + 12, forecasts[dayIndex].iconMeteoCon);
  //预报 温度
  String temp = String(forecasts[dayIndex].tmp, 0) + "°C";
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, temp);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}
//绘制页眉
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[14];
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);

  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 54, String(buff));
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String temp = String(currentWeather.tmp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(128, 54, temp);
  display->drawHorizontalLine(0, 52, 128);
}
//开始更新
void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}

void WiFiConfig() {
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  // Uncomment for testing wifi manager
  //wifiManager.resetSettings();
  wifiManager.setAPCallback(configModeCallback);

  //or use this for auto generated name ESP + ChipID
  wifiManager.autoConnect();

  //Manual Wifi
  //WiFi.begin(WIFI_SSID, WIFI_PWD);
  String hostname(HOSTNAME);
  hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(hostname);


  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.clear();
    display.drawString(64, 10, "Connecting to WiFi");

    display.display();

    counter++;
  }

  Serial.println("WiFi Connected!");
  Serial.print("IP ssid: ");
  Serial.println(WiFi.SSID());
  Serial.print("IP addr: ");
  Serial.println(WiFi.localIP());
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "Wifi Manager");
  display.drawString(64, 20, "Please connect to AP");
  display.drawString(64, 30, myWiFiManager->getConfigPortalSSID());
  display.drawString(64, 40, "To setup Wifi Configuration");
  display.display();
}

//绘制日期时间
void drawBigTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[16];


  display->setTextAlignment(TEXT_ALIGN_CENTER);
//  display->setFont(ArialMT_Plain_10);
//  String date = WDAY_NAMES[timeInfo->tm_wday];
//  //年-月-日 星期
//  sprintf_P(buff, PSTR("%04d-%02d-%02d %s"),timeInfo->tm_year + 1900,timeInfo->tm_mon + 1,timeInfo->tm_mday, WDAY_NAMES[timeInfo->tm_wday].c_str() );
//  display->drawString(64 + x, 5 + y, String(buff));
  //大字体
  display->setFont(Dialog_plain_48);
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);
  display->drawString(64 + x, 1 + y, String(buff));
  display->setTextAlignment(TEXT_ALIGN_CENTER);

//  display->setFont(ArialMT_Plain_10);
//  display->setTextAlignment(TEXT_ALIGN_CENTER);
//  display->drawString(64 + x, 38 + y, "HUM: "+String(currentWeather.hum)+"%");
  
}
