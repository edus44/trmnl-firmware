#include "temp_sensor.h"

#if defined(BOARD_SEEED_RETERMINAL_E1001)

#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <trmnl_log.h>
#include "http_client.h"
#include <config.h>

#define I2C_SDA_PIN 19
#define I2C_SCL_PIN 20
#define TEMP_SENSOR_ADDR 0x44
#define SHT40_CMD_MEASURE 0xFD

#define HA_URL "http://192.168.1.221:8123/api/states/sensor.miterm"
#define HA_BEARER_TOKEN "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiIyN2RjMWI4ZDRjZDM0NDUyYjE0ZWNiOTNiNjQyOTYxZSIsImlhdCI6MTc2ODc1NjA4OCwiZXhwIjoyMDg0MTE2MDg4fQ.6ybVWxzxXaeo289qmuBFQT_yJmiYjVOCWIKnSkSWtd8"

static bool initialized = false;

static void submitToHomeAssistant(float battery, float temperature, float humidity, bool hasSensorData) {
    JsonDocument doc;
    doc["state"] = battery;
    JsonObject attrs = doc["attributes"].to<JsonObject>();
    if (hasSensorData) {
        attrs["temperature"] = temperature;
        attrs["humidity"] = humidity;
    }
    String payload;
    serializeJson(doc, payload);

    withHttp(HA_URL, [&](HTTPClient *http, HttpError err) -> bool {
        if (err != HTTPCLIENT_SUCCESS || !http) {
            Log_error("HA HTTP connect failed");
            return false;
        }
        http->addHeader("Content-Type", "application/json");
        http->addHeader("Authorization", "Bearer " HA_BEARER_TOKEN);
        int code = http->POST(payload);
        if (code < 0) {
            Log_error("HA POST failed: %d", code);
            return false;
        }
        Log_info("HA sent (bat=%.2f, temp=%.2f, hum=%.2f), HTTP %d", battery, temperature, humidity, code);
        return true;
    });
}

static uint8_t crc8(const uint8_t *data, int len) {
    uint8_t crc = 0xFF;
    for (int j = len; j; --j) {
        crc ^= *data++;
        for (int i = 8; i; --i)
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
    }
    return crc;
}

static float readBattery(void) {
    pinMode(PIN_VBAT_SWITCH, OUTPUT);
    digitalWrite(PIN_VBAT_SWITCH, VBAT_SWITCH_LEVEL);
    delay(10);
    
    int32_t adc = 0;
    analogRead(PIN_BATTERY);
    for (uint8_t i = 0; i < 8; i++) {
        adc += analogReadMilliVolts(PIN_BATTERY);
    }
    
    digitalWrite(PIN_VBAT_SWITCH, (VBAT_SWITCH_LEVEL == HIGH ? LOW : HIGH));
    float voltage = (adc / 8) * 2 / 1000.0f;
    
    // Convert to percentage (0-1) based on LiPo range 3.0V-4.2V
    float percent = (voltage - 3.0f) / (4.2f - 3.0f);
    if (percent < 0) percent = 0;
    if (percent > 1) percent = 1;
    return percent;
}

void temp_sensor_read_and_submit(void) {
    // Initialize on first call
    if (!initialized) {
        Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
        Wire.setClock(100000);
        initialized = true;
        Log_info("SHT40 I2C initialized (SDA=%d, SCL=%d)", I2C_SDA_PIN, I2C_SCL_PIN);
    }

    float temp = 0, hum = 0;
    bool sensorOk = false;

    // Read sensor
    Wire.beginTransmission(TEMP_SENSOR_ADDR);
    Wire.write(SHT40_CMD_MEASURE);
    if (Wire.endTransmission() == 0) {
        delay(10);
        if (Wire.requestFrom((uint8_t)TEMP_SENSOR_ADDR, (uint8_t)6) == 6) {
            uint8_t raw[6];
            for (int i = 0; i < 6; i++) raw[i] = Wire.read();

            if (crc8(raw, 2) == raw[2] && crc8(raw + 3, 2) == raw[5]) {
                temp = -45.0f + 175.0f * ((raw[0] << 8) | raw[1]) / 65535.0f;
                hum = -6.0f + 125.0f * ((raw[3] << 8) | raw[4]) / 65535.0f;
                if (hum < 0) hum = 0; if (hum > 100) hum = 100;
                sensorOk = true;
                Log_info("SHT40: %.2f°C, %.2f%%", temp, hum);
            } else {
                Log_error("SHT40 CRC error");
            }
        } else {
            Log_error("SHT40 read failed");
        }
    } else {
        Log_error("SHT40 not responding");
    }

    // Read battery
    float battery = readBattery();
    Log_info("Battery: %.2f%%", battery * 100);

    // Submit to Home Assistant
    submitToHomeAssistant(battery, temp, hum, sensorOk);
}

#else
void temp_sensor_read_and_submit(void) {}
#endif
