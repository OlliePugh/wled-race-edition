#pragma once

#include "wled.h"

#include <WiFi.h>
#include <HTTPClient.h>

#include "car.h"
#include "location_service.h"

#define CARS_BUFFER_SIZE 30

// IF THE BELOW CHANGES THE SORTING WILL NEED TO BE UPDATED IN THE UI
static const char _data_FX_MODE_LIVE_RACE[] PROGMEM = "Live Race";

int percentageToLedIndex(float percentage)
{
  int stripLength = strip.getLength();
  float ledPosition = ((percentage / 100) * stripLength);

  int ledIndex = ledPosition;

  ledIndex = max(0, min(stripLength - 1, ledIndex));

  return ledIndex;
}

class LiveTrackUserMod : public Usermod
{
private:
  static LiveTrackUserMod *instance;
  unsigned long lastRequestTime = 0;
  const unsigned long requestInterval = 10000; // 10 seconds
  bool timeOffsetSet = false;
  // I really tried to use a hashtable here but the arduino library is ass
  LocationService &locationService;
  TimeService &timeService;
  LiveTrackUserMod(LocationService &locService, TimeService &timeServ)
      : locationService(locService), timeService(timeServ)
  {
    for (int i = 0; i < CARS_BUFFER_SIZE; i++)
    {
      cars[i] = nullptr;
    }
  }

  static void httpRequestTask(void *pvParameters)
  {
    LiveTrackUserMod *mod = static_cast<LiveTrackUserMod *>(pvParameters);
    LocationDto locationBuffer[1500];
    DriverDto driverBuffer[30];
    while (true)
    {
      int newLocations = mod->locationService.fetchData(locationBuffer, driverBuffer);
      mod->updateLocations(locationBuffer, driverBuffer, newLocations);
      vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay for 1 second
    }
  }

  Car *getCarByNumber(int driverNumber)
  {
    for (int i = 0; i < CARS_BUFFER_SIZE; i++)
    {
      if (cars[i] != nullptr && cars[i]->driverNumber == driverNumber)
      {
        return cars[i];
      }
    }
    Serial.print("car not available with number ");
    Serial.println(driverNumber);
    return nullptr;
  }

  void updateLocations(LocationDto *locations, DriverDto *drivers, int amountInBuffer)
  {
    if (!timeOffsetSet && amountInBuffer > 0)
    {
      timeService.setServerTime(locations[0].occuredAt);
      Serial.print("Setting server time to ");
      Serial.println(locations[0].occuredAt);
      timeOffsetSet = true;
    }

    for (size_t i = 0; i < amountInBuffer; i++)
    {
      LocationDto currentLocation = locations[i];

      Car *car = getCarByNumber(currentLocation.driverNumer);

      // TODO: check if the driver number exists
      if (car == nullptr)
      {
        // find the driver
        DriverDto driver;
        for (size_t j = 0; j < sizeof(drivers); j++)
        {
          if (drivers[j].driverNumber == currentLocation.driverNumer)
          {
            driver = drivers[j];
            break;
          }
        }
        Serial.println("creating new driver");
        car = new Car(currentLocation.driverNumer, driver.color, timeService);
        // add the car to the buffer
        for (int i = 0; i < CARS_BUFFER_SIZE; i++)
        {
          if (cars[i] == nullptr)
          {
            cars[i] = car;
            break;
          }
        }
      }

      if (car != nullptr)
      {
        car->addLocation(currentLocation.occuredAt, currentLocation.position);
      }
      else
      {
        Serial.print("Could not find car with number");
        Serial.println(currentLocation.driverNumer);
      }
    }
  }

public:
  Car *cars[CARS_BUFFER_SIZE];
  static LiveTrackUserMod *getInstance()
  {
    return instance;
  }

  static LiveTrackUserMod *withDefaults()
  {
    LocationService *locationService = new LocationService(new ESP32Time(), new WiFiClient());
    TimeService *timeService = new TimeService(new ESP32Time());
    Serial.println("Creating new instance");
    return new LiveTrackUserMod(*locationService, *timeService);
  }

  uint16_t mode_live_race()
  {
    // set to white
    strip.fill(0x323232);
    if (instance == nullptr)
    {
      strip.fill(0xff00ff);
      Serial.println("instance is null");
      return FRAMETIME;
    }

    for (int i = 0; i < CARS_BUFFER_SIZE; i++)
    {
      if (instance->cars[i] != nullptr)
      {
        float loc = instance->cars[i]->getLocation();
        if (loc != -1)
        {
          strip.setPixelColor(percentageToLedIndex(loc), instance->cars[i]->color);
        }
      }
    }
    return FRAMETIME;
  }

  static uint16_t mode_live_race_static()
  {
    if (instance != nullptr)
    {
      return instance->mode_live_race();
    }
    return FRAMETIME;
  }

  void setup()
  {
    Serial.begin(115200);
    Serial.println("Live Track Usermod setup");
    instance = this;
    strip.addEffect(255, &mode_live_race_static, _data_FX_MODE_LIVE_RACE);
    xTaskCreate(httpRequestTask, "HTTP Request Task", 32768, this, 1, NULL);
  }

  void loop()
  {
    if (millis() - lastRequestTime > requestInterval)
    {
      Serial.println("still looping okay");
      lastRequestTime = millis();
    }
  }
};

// Define the static member variable
LiveTrackUserMod *LiveTrackUserMod::instance = nullptr;