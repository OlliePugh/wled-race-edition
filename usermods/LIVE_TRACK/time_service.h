#include <Arduino.h>
#include <ESP32Time.h>

#ifndef TIMESERVICE_H
#define TIMESERVICE_H

#define ENDPOINT "time.google.com"

class TimeService
{
private:
    static int const OFFSET = 0;

public:
    TimeService(ESP32Time *rtc)
    {
        this->rtc = rtc;
    }
    uint64_t getTime()
    {
        return static_cast<uint64_t>(rtc->getEpoch()) * 1000ULL + rtc->getMillis();
    }
    void setServerTime(uint64_t currentServerTime)
    {
        // TODO: fix this piece of shit
        rtc->setTime(currentServerTime / 1000ULL, (currentServerTime % 1000ULL));
    }
    void setup()
    {
        configTime(TimeService::OFFSET, 0, ENDPOINT);
        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
        {
            rtc->setTimeStruct(timeinfo);
        }
    }

private:
    ESP32Time *rtc;
};
#endif // TIMESERVICE_H