#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include "time_service.h"

struct Location
{
    uint64_t occurredAt;
    float position;
};

class Car
{
private:
    static const int MAX_LOCATIONS = 16;
    Location *locations[MAX_LOCATIONS];
    SemaphoreHandle_t xMutex;
    float lastRequestedLocation = -1;

public:
    uint8_t driverNumber;
    CRGB color;
    TimeService &timeService;
    Car(uint8_t _driverNumber, CRGB _color, TimeService &_timeService) : driverNumber(_driverNumber), color(_color), timeService(_timeService)
    {
        xMutex = xSemaphoreCreateMutex();
        for (int i = 0; i < MAX_LOCATIONS; i++)
        {
            locations[i] = nullptr;
        }
    }

    void removeOutdatedLocations()
    {
        if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
        {
            // get the current time from the rtc
            uint64_t currentTime = timeService.getTime();

            // Remove outdated locations
            int lastValidIndex = -1;
            for (int i = 0; i < MAX_LOCATIONS; i++)
            {
                if (locations[i] != nullptr && locations[i]->occurredAt < currentTime)
                {
                    lastValidIndex = i;
                }
            }

            for (int i = 0; i < lastValidIndex; i++)
            {
                if (locations[i] != nullptr)
                {
                    // Serial.println("removing location because it is in the past");
                    delete locations[i];
                    locations[i] = nullptr;
                }
            }

            // Shift all non-null pointers to the left
            int shiftIndex = 0;
            for (int i = 0; i < MAX_LOCATIONS; i++)
            {
                if (locations[i] != nullptr)
                {
                    locations[shiftIndex++] = locations[i];
                }
            }

            // Set the remaining pointers to nullptr
            for (int i = shiftIndex; i < MAX_LOCATIONS; i++)
            {
                locations[i] = nullptr;
            }

            xSemaphoreGive(xMutex);
        }
        else
        {
            Serial.println("could not take mutex");
        }
    }

    void addLocation(uint64_t occurredAt, float position)
    {
        removeOutdatedLocations();
        // if the data is in the past we can ignore it
        if (occurredAt < timeService.getTime())
        {
            return;
        }

        if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
        {
            // check if this location is already in the queue
            for (int i = 0; i < MAX_LOCATIONS; i++)
            {
                if (locations[i] != nullptr &&
                    locations[i]->occurredAt == occurredAt &&
                    locations[i]->position == position)
                {
                    // Serial.println("Location already exists in the queue");
                    // nothing to do
                    xSemaphoreGive(xMutex);
                    return;
                }
            }

            bool successfull = false;
            // add it to the end of the locations queue
            for (int i = 0; i < MAX_LOCATIONS; i++)
            {
                if (locations[i] == nullptr)
                {
                    locations[i] = new Location{occurredAt, position};
                    successfull = true;
                    break;
                }
            }

            if (!successfull)
            {
                Serial.println("Buffer full");
            }

            xSemaphoreGive(xMutex);
        }
        else
        {
            Serial.println("could not take mutex");
        }
    }

    float getLocation()
    {
        removeOutdatedLocations();
        if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
        {
            // get last location
            Location *previous = locations[0];
            Location *next = locations[1];

            if (previous == nullptr)
            {
                xSemaphoreGive(xMutex);
                return lastRequestedLocation;
            }

            if (next == nullptr)
            {
                float position = previous->position;
                xSemaphoreGive(xMutex);
                lastRequestedLocation = position;
                return position;
            }

            // get the current time from the rtc
            uint64_t currentTime = timeService.getTime();
            Location *loc = nullptr;
            // check if the current time is between the two locations
            if (previous->occurredAt <= currentTime && currentTime <= next->occurredAt)
            {
                // calculate the position between the two locations
                float position;
                if (previous->position > 90 && next->position < 10)
                {
                    // handle wrap-around from 99 to 1
                    position = previous->position + (next->position + 100 - previous->position) * (currentTime - previous->occurredAt) / (next->occurredAt - previous->occurredAt);
                    if (position >= 100)
                    {
                        position -= 100;
                    }
                }
                else
                {
                    position = previous->position + (next->position - previous->position) * (currentTime - previous->occurredAt) / (next->occurredAt - previous->occurredAt);
                }
                xSemaphoreGive(xMutex);
                lastRequestedLocation = position;
                return position;
            }
            xSemaphoreGive(xMutex);
        }

        return lastRequestedLocation;
    }

    void debug()
    {
        Serial.print("Driver number: ");
        Serial.println(driverNumber);
        Serial.print("====== LOCATION DUMP (CAR ");
        Serial.print(driverNumber);
        Serial.println(") ======");

        if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
        {
            // Print the locations from the array
            for (int i = 0; i < MAX_LOCATIONS; i++)
            {
                if (locations[i] != nullptr)
                {
                    Serial.print("occurred at: ");
                    Serial.print(locations[i]->occurredAt);
                    Serial.print(" position: ");
                    Serial.println(locations[i]->position);
                }
            }

            xSemaphoreGive(xMutex);
        }
        else
        {
            Serial.println("could not take mutex");
        }
        Serial.println("====== END LOCATION DUMP ======");
    }
};
;