#include "weather_draw_functions.h"
#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Fonts/CourierCyr10.h>
#include <Fonts/CourierCyr12.h>
#include <Fonts/CourierCyr14.h>
#include <Fonts/CourierCyr16.h>
#include <Fonts/CourierCyr18.h>
#include <Fonts/CourierCyr6.h>
#include <Fonts/CourierCyr7.h>
#include <Fonts/CourierCyr8.h>
#include <Fonts/CourierCyr9.h>
#include <Fonts/TimesNRCyr10.h>
#include <Fonts/TimesNRCyr12.h>
#include <Fonts/TimesNRCyr14.h>
#include <Fonts/TimesNRCyr16.h>
#include <Fonts/TimesNRCyr18.h>
#include <Fonts/TimesNRCyr7.h>
#include <Fonts/TimesNRCyr8.h>
#include <Fonts/TimesNRCyr9.h>
#include <Fonts/timesnrcyr6.h>
#include <Fonts/FreeSans24pt7b.h>
#include <GxEPD2_3C.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <time.h>

// LANGUAGE SELECTION
const char* language = "en"; // "en" or "ru"

// WiFi and API credentials
String apiKey = "99fc51e4e132d3e0a465294f293ad82a";
String city = "Wroclaw";
String weatherUrl = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "&appid=" + apiKey + "&units=metric&lang=" + language;
String forecastUrl = "http://api.openweathermap.org/data/2.5/forecast?q=" + city + "&appid=" + apiKey + "&units=metric&lang=" + language;

const char* ssid = "bogswifi";
const char* password = "bog12345";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3 * 3600; // GMT+1 for Minsk
const int daylightOffset_sec = 3600;

// --- LANGUAGE STRINGS ---
struct LangStrings {
    const char* forecast;
    const char* events;
    const char* dayLetters[7];
    const char* updatedAt;
};

const LangStrings enStrings = {
    "Forecast",
    "Events",
    { "M", "T", "W", "T", "F", "S", "S" },
    "Updated: %d.%m.%Y %H:%M"
};

const LangStrings ruStrings = {
    "Прогноз",
    "События",
    { "П", "В", "С", "Ч", "П", "С", "В" },
    "Обновлено: %d.%m.%Y %H:%M"
};

const LangStrings* s = &enStrings; // Pointer to current language strings

// Month name translations
const char* month_names_en[12] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

const char* month_names_ru[12] = {
    "Январь", "Февраль", "Март", "Апрель", "Май", "Июнь",
    "Июль", "Август", "Сентябрь", "Октябрь", "Ноябрь", "Декабрь"
};

const char* day_names_en[7] = {
    "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"
};

const char* day_names_ru[7] = {
    "Понедельник", "Вторник", "Среда", "Четверг", "Пятница", "Суббота", "Воскресенье"
};

// E-paper display pins
#define EPD_MOSI 11
#define EPD_SCK 12
#define EPD_CS 10
#define EPD_DC 18
#define EPD_RST 17
#define EPD_BUSY 16

GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT / 4> display(GxEPD2_750c_Z08(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

struct CurrentWeather {
    String temperature;
    String description;
    String icon;
};

struct Forecast {
    String dayOfMonth;
    String temp;
    String icon;
};

struct Event {
    String time;
    String title;
};

CurrentWeather currentWeather;
Forecast forecastList[5];
Event eventList[] = {
    { "14:00", "Встреча" },
    { "19:00", "Кино" },
    { "22:00", "Друзья" }
};

void parseWeather(String json)
{
    JsonDocument doc;
    deserializeJson(doc, json);
    double temp_val = doc["main"]["temp"];
    // Use the degree symbol in a way that works with both locales
    currentWeather.temperature = String(temp_val, 0) + " C"; // Use 'C' only to avoid encoding issues with Russian locale
    currentWeather.description = doc["weather"][0]["description"].as<String>();
    currentWeather.icon = doc["weather"][0]["icon"].as<String>();
}

void parseForecast(String json)
{
    JsonDocument doc;
    deserializeJson(doc, json);
    JsonArray list = doc["list"];

    time_t now;
    struct tm* timeinfo;
    time(&now);
    timeinfo = localtime(&now);
    int today_yday = timeinfo->tm_yday;

    int day_index = 0;
    int last_day = -1;

    for (int i = 0; i < list.size() && day_index < 5; i++) {
        long timestamp = list[i]["dt"];
        struct tm forecast_timeinfo;
        gmtime_r(&timestamp, &forecast_timeinfo);

        if (forecast_timeinfo.tm_yday != today_yday && forecast_timeinfo.tm_yday != last_day) {
            last_day = forecast_timeinfo.tm_yday;
            char day_buf[3];
            strftime(day_buf, sizeof(day_buf), "%d", &forecast_timeinfo);

            forecastList[day_index].dayOfMonth = String(day_buf);
            forecastList[day_index].temp = String(list[i]["main"]["temp"].as<double>(), 0) + "C"; // Use 'C' only to avoid encoding issues with Russian locale
            forecastList[day_index].icon = list[i]["weather"][0]["icon"].as<String>();
            day_index++;
        }
    }
}

void fetchWeatherData()
{
    HTTPClient http;
    http.begin(weatherUrl);
    int httpCode = http.GET();
    if (httpCode == 200)
        parseWeather(http.getString());
    http.end();

    http.begin(forecastUrl);
    httpCode = http.GET();
    if (httpCode == 200)
        parseForecast(http.getString());
    http.end();
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

void drawWeather(const String& city, const CurrentWeather& weather)
{
    int16_t tbx, tby;
    uint16_t tbw, tbh;

    display.setFont(&TimesNRCyr12pt8b);
    display.getTextBounds(city, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((400 - tbw) / 2, 20);
    display.print(city);

    display.setTextColor(GxEPD_RED);
    // Use draw_temp function for temperature with Celsius mark (moved up)
    draw_temp((400 - 100) / 2, 90, 4, currentWeather.temperature.toFloat(), &FreeSans24pt7b); // Increased circle size from 2 to 4
    display.setTextColor(GxEPD_BLACK);

    // Use vector weather icon instead of bitmap (moved up)
    draw_wx_icon((400 / 2), 130, weather.icon, LargeIcon);

    display.setFont(&TimesNRCyr12pt8b);
    display.getTextBounds(weather.description, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((400 - tbw) / 2, 180);
    display.print(weather.description);
}

void drawTime(struct tm& timeinfo)
{
    const char* day_name;
    if (strcmp(language, "ru") == 0) {
        day_name = day_names_ru[timeinfo.tm_wday];
    } else {
        day_name = day_names_en[timeinfo.tm_wday];
    }

    // Format date with translated month
    const char* month_name;
    if (strcmp(language, "ru") == 0) {
        month_name = month_names_ru[timeinfo.tm_mon];
    } else {
        month_name = month_names_en[timeinfo.tm_mon];
    }

    char date_buffer[64];
    sprintf(date_buffer, "%d, %s, %d", timeinfo.tm_mday, month_name, timeinfo.tm_year + 1900);

    int16_t tbx, tby;
    uint16_t tbw, tbh;
    display.setFont(&TimesNRCyr12pt8b);

    display.getTextBounds(date_buffer, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((400 - tbw) / 2, 210);
    display.print(date_buffer);

    display.getTextBounds(day_name, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((400 - tbw) / 2, 240);
    display.print(day_name);
}

void drawCalendar(struct tm& timeinfo)
{
    const char* month_name;
    if (strcmp(language, "ru") == 0) {
        month_name = month_names_ru[timeinfo.tm_mon];
    } else {
        month_name = month_names_en[timeinfo.tm_mon];
    }

    int16_t tbx, tby;
    uint16_t tbw, tbh;
    display.setFont(&TimesNRCyr12pt8b);

    // Draw month title centered on right side
    display.getTextBounds(month_name, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setTextColor(GxEPD_RED);
    display.setCursor(400 + (400 - tbw) / 2, 40);
    display.print(month_name);
    display.setTextColor(GxEPD_BLACK);

    display.fillRect(420, 60, 360, 2, GxEPD_BLACK);

    int month = timeinfo.tm_mon;
    int year = timeinfo.tm_year + 1900;
    int today = timeinfo.tm_mday;

    int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        daysInMonth[1] = 29;
    }

    struct tm firstDayOfMonth = { 0, 0, 0, 1, month, year - 1900 };
    mktime(&firstDayOfMonth);
    int startDay = (firstDayOfMonth.tm_wday + 6) % 7;

    display.setFont(&CourierCyr9pt8b);
    for (int i = 0; i < 7; i++) {
        display.setCursor(420 + i * 50, 80);
        display.print(s->dayLetters[i]);
    }

    int day = 1;
    for (int row = 0; row < 6; row++) {
        for (int col = 0; col < 7; col++) {
            if ((row == 0 && col < startDay) || day > daysInMonth[month]) {
                continue;
            }
            int x = 420 + col * 50;
            int y = 110 + row * 25;

            display.setFont(&CourierCyr9pt8b);

            // Check if current day is a weekend day (Saturday or Sunday)
            // col 0-6 represents Monday-Sunday, so weekend is col 5 (Saturday) and col 6 (Sunday)
            bool isWeekend = (col == 5 || col == 6); // Saturday or Sunday

            if (isWeekend) {
                // For weekend days, draw text multiple times to create a bold effect
                // Draw the text multiple times in slightly different positions
                for (int offset_x = 0; offset_x <= 1; offset_x++) {
                    for (int offset_y = 0; offset_y <= 1; offset_y++) {
                        display.setCursor(x + offset_x, y + offset_y);
                        display.print(day);
                    }
                }
            } else {
                // For regular days, draw normal text
                display.setCursor(x, y);
                display.print(day);
            }

            if (day == today) {
                display.drawRect(x - 5, y - 15, 30, 20, GxEPD_RED);
            }
            day++;
        }
    }
}

void drawForecast(struct tm& timeinfo)
{
    int16_t tbx, tby;
    uint16_t tbw, tbh;

    // Add "Updated at" text with full date
    char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), s->updatedAt, &timeinfo);

    display.setFont(&TimesNRCyr7pt8b); // Using a small font
    display.getTextBounds(time_buffer, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor(20, 260); // Position it on the left, above the forecast line (moved up)
    display.print(time_buffer);

    display.setFont(&TimesNRCyr12pt8b);

    // Draw top divider line
    display.fillRect(20, 265, 360, 2, GxEPD_BLACK);

    // Draw forecast title centered
    display.getTextBounds(s->forecast, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setTextColor(GxEPD_RED); // Make forecast header red like other headers
    display.setCursor((400 - tbw) / 2, 280);
    display.print(s->forecast);
    display.setTextColor(GxEPD_BLACK); // Reset to black for the rest of the content

    // Draw bottom divider line
    display.fillRect(20, 290, 360, 2, GxEPD_BLACK);

    for (int i = 0; i < 5; i++) {
        int x_base = 20 + i * 75;
        display.setFont(&CourierCyr9pt8b);
        display.getTextBounds(forecastList[i].dayOfMonth.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
        display.setCursor(x_base + (32 - tbw) / 2, 310);
        display.print(forecastList[i].dayOfMonth);

        // Use vector weather icon instead of bitmap (positioned higher with spacing)
        draw_wx_icon(x_base + 16, 330, forecastList[i].icon, SmallIcon);

        // Use draw_temp function for forecast temperatures with Celsius mark (positioned lower with spacing)
        float tempValue = forecastList[i].temp.toFloat();
        draw_temp(x_base + 4, 355, 1, tempValue, &CourierCyr9pt8b);
    }
}

void drawEvents()
{
    display.setFont(&TimesNRCyr12pt8b);

    // Draw top divider line
    display.fillRect(420, 260, 360, 2, GxEPD_BLACK);

    // Draw events title centered on right side
    int16_t tbx, tby;
    uint16_t tbw, tbh;
    display.getTextBounds(s->events, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setTextColor(GxEPD_RED);
    display.setCursor(400 + (400 - tbw) / 2, 280);
    display.print(s->events);
    display.setTextColor(GxEPD_BLACK);

    // Draw bottom divider line
    display.fillRect(420, 290, 360, 2, GxEPD_BLACK);

    display.setFont(&CourierCyr9pt8b);
    for (int i = 0; i < sizeof(eventList) / sizeof(Event); i++) {
        display.setCursor(420, 320 + i * 30);
        display.print(eventList[i].time + " " + eventList[i].title);
    }
}

void drawDashboard()
{
    fetchWeatherData();
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }

    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);

        drawWeather(city, currentWeather);
        drawTime(timeinfo);
        drawForecast(timeinfo);
        drawCalendar(timeinfo);
        drawEvents();

        display.fillRect(398, 0, 4, 480, GxEPD_BLACK);

    } while (display.nextPage());
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Init e-paper...");

    if (strcmp(language, "ru") == 0) {
        s = &ruStrings;
        // Note: Arduino doesn't fully support setlocale for month names
        // Instead we'll handle month translations manually in drawCalendar
    } else {
        s = &enStrings;
    }

    connectWiFi();
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    display.init(115200);
    display.setRotation(0);

    drawDashboard();
    
    // Put ESP32 into deep sleep for 1 hours (1 * 60 * 60 * 1,000,000 microseconds)
    Serial.println("Entering deep sleep for 1 hours...");
    esp_sleep_enable_timer_wakeup(1 * 60 * 60 * 1000000ULL); // 1 hours in microseconds
    esp_deep_sleep_start();
}

void loop()
{
    // Empty loop - everything happens in setup() due to deep sleep
}
