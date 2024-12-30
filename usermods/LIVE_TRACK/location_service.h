#pragma once

#include <Arduino.h>
#include <ESP32Time.h>
#include <HTTPClient.h>

#define HTTP_ENDPOINT "http://192.168.1.85:8080/locations"

#define COLOUR_IDENTIFIER "c"
#define DATE_IDENTIFIER "d"
#define DRIVER_NUMBER_IDENTIFIER "n"
#define LOCATION_IDENTIFIER "l"

struct LocationDto
{
    int driverNumer;
    float position;
    uint64_t occuredAt;
};

struct DriverDto
{
    uint8_t driverNumber;
    CRGB color;
};

class LocationService
{
public:
    LocationService(
        ESP32Time *rtc,
        WiFiClient *client)
    {
        this->rtc = rtc;
        this->client = client;
    }

    int fetchData(LocationDto *locations, DriverDto *driverBuffer)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("WiFi not connected");
            return 0;
        }
        HTTPClient http;
        http.useHTTP10(true);
        http.begin(*client, HTTP_ENDPOINT);
        int httpResponseCode = http.GET();

        int i = 0;

        if (httpResponseCode > 0)
        {
            Serial.print("Location HTTP Response code: ");
            Serial.println(httpResponseCode);
            PSRAMDynamicJsonDocument doc(51200);
            deserializeJson(doc, http.getStream());
            uint64_t baselineDate = doc["baseline"].as<uint64_t>();
            Serial.print("baseline date: ");
            Serial.println(baselineDate);
            JsonArray jsonLocations = doc["locations"].as<JsonArray>();
            JsonArray jsonDrivers = doc["drivers"].as<JsonArray>();
            // String output = "";
            // serializeJson(doc, output);
            // Serial.println(output);

            for (JsonVariant location : jsonLocations)
            {
                locations[i].driverNumer = location[DRIVER_NUMBER_IDENTIFIER].as<int>();
                locations[i].position = location[LOCATION_IDENTIFIER].as<float>();
                locations[i].occuredAt = location[DATE_IDENTIFIER].as<uint64_t>() + baselineDate;
                i++;
            }
            int j = 0;
            for (JsonVariant driver : jsonDrivers)
            {
                String colorString = driver[COLOUR_IDENTIFIER].as<String>();
                int r = strtol(colorString.substring(0, 2).c_str(), NULL, 16);
                int g = strtol(colorString.substring(2, 4).c_str(), NULL, 16);
                int b = strtol(colorString.substring(4, 6).c_str(), NULL, 16);
                CRGB color = CRGB(r, g, b);
                driverBuffer[j].driverNumber = driver[DRIVER_NUMBER_IDENTIFIER].as<int>();
                driverBuffer[j].color = color;
                j++;
            }

            doc.clear();
        }
        else
        {
            Serial.print("Location error code: ");
            Serial.println(httpResponseCode);
        }
        http.end();
        Serial.print("Location data fetched: ");
        Serial.println(i);
        return i;
    }

private:
    ESP32Time *rtc;
    WiFiClient *client;
};