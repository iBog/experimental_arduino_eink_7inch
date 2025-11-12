#include <Adafruit_GFX.h>

#include <Arduino.h>

#include <ArduinoJson.h>

#include <Fonts/FreeMonoBold12pt7b.h>

#include <Fonts/FreeMonoBold9pt7b.h>

#include <GxEPD2_3C.h>

#include <HTTPClient.h>

#include <WiFi.h>

#include <time.h>



String apiKey = "99fc51e4e132d3e0a465294f293ad82a";

String city = "Minsk";

String url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "&appid=" + apiKey + "&units=metric";



const char* ssid = "bogswifi";

const char* password = "bog12345";

const char* ntpServer = "pool.ntp.org";

const long gmtOffset_sec = 3 * 3600; // adjust for your timezone (GMT+3 for Minsk)

const int daylightOffset_sec = 0; // daylight saving offset if needed



// Пины подключения к DESPI-C02

#define EPD_MOSI 11 // SOI

#define EPD_SCK 12 // SCK

#define EPD_CS 10 // CS

#define EPD_DC 18 // DC

#define EPD_RST 17 // RESET

#define EPD_BUSY 16 // BUSY



// Объявляем объект дисплея (3-цветный, 800x480)

// // Объявление с буфером в PSRAM

// GxEPD2_3C<GxEPD2_800x480, GxEPD2_800x480::HEIGHT> display(

//   GxEPD2_800x480(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY,

//                  /*enableDiagnostic=*/ true,

//                  /*usePartialUpdateWindow=*/ false,

//                  /*useFastFullUpdate=*/ true,

//                  /*usePartialUpdate=*/ true,

//                  /*usePagedUpdate=*/ false)

// );

GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT / 4> display(GxEPD2_750c_Z08(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// GDEW075Z08 800x480, GD7965



String weatherText = "";

String tempText = "";

void parseWeather(String json)

{

    DynamicJsonDocument doc(1024);

    deserializeJson(doc, json);

    float temp = doc["main"]["temp"];

    const char* desc = doc["weather"][0]["description"];

    tempText = "Temp: " + String(temp, 1) + " C";

    weatherText = String(desc);

}



String getWeather()

{

    HTTPClient http;

    http.begin(url);

    int httpCode = http.GET();

    String payload = "";

    if (httpCode == 200) {

        payload = http.getString();

    }

    http.end();

    return payload;

}



String getTimeString()

{

    struct tm timeinfo;

    if (!getLocalTime(&timeinfo)) {

        return "Time: Error";

    }

    char buf[32];

    strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);

    return "Time: " + String(buf);

}



void connectWiFi()

{

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {

        delay(500);

        Serial.print(".");

    }

    Serial.println("WiFi connected");

}



void setup()

{

    Serial.begin(115200);

    Serial.println("Init e-paper...");

    String textFlash = "Flash: " + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB";

    String textPsRam = "PSRAM: " + String(ESP.getPsramSize() / 1024 / 1024) + " MB";

    Serial.println(textFlash);

    Serial.println(textPsRam);

    connectWiFi();

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // Fetch weather

    String json = getWeather();

    parseWeather(json);

    String timeText = getTimeString();



    // Инициализация дисплея

    display.init(115200);

    display.setRotation(0); // ориентация (0–3)



    // Очистка экрана

    display.setFullWindow();

    display.firstPage();

    do {

        display.fillScreen(GxEPD_WHITE);



        // display.drawRect(0, 0, 800, 480, GxEPD_BLACK);

        // display.drawRect(5, 5, 790, 470, GxEPD_RED);



        // Текст

        display.setTextColor(GxEPD_BLACK);

        display.setFont(&FreeMonoBold12pt7b);

        display.setCursor(50, 50);

        display.print("ESP32-S3!");

        display.setCursor(50, 70);

        display.print(textFlash);

        display.setCursor(50, 95);

        display.print(textPsRam);



        display.setTextColor(GxEPD_RED);

        display.setCursor(50, 115);

        display.print("Red layer text");



        // Простая графика

        display.drawRect(40, 150, 250, 100, GxEPD_RED);

        // display.drawLine(40, 150, 240, 250, GxEPD_RED);



        display.setFont(&FreeMonoBold12pt7b);

        display.setTextColor(GxEPD_BLACK);

        display.setCursor(50, 170);

        display.print(tempText);

        display.setCursor(50, 190);

        display.print(weatherText);

        display.setFont(&FreeMonoBold9pt7b);

        display.setTextColor(GxEPD_BLACK);

        display.setCursor(50, 210);

        display.print(timeText);



    } while (display.nextPage());



    Serial.println("Done.");



    // Обновление только области 200x200 пикселей

    // display.setPartialWindow(100, 100, 200, 200); // x, y, width, height

    // display.fillScreen(GxEPD_WHITE);

    // display.setCursor(120, 150);

    // display.print("Partial Update");

    // display.updateWindow(true); // true = монохромный режим (быстрее)

}



void loop()

{

    // пусто — дисплей обновляется только в setup()

}