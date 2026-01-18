#include "temp_sensor.h"

#if defined(BOARD_SEEED_RETERMINAL_E1001)

#include <Wire.h>
#include <trmnl_log.h>

#define I2C_SDA_PIN 19
#define I2C_SCL_PIN 20
#define TEMP_SENSOR_ADDR 0x44
#define SHT40_CMD_MEASURE 0xFD

static bool initialized = false;

static uint8_t crc8(const uint8_t *data, int len) {
    uint8_t crc = 0xFF;
    for (int j = len; j; --j) {
        crc ^= *data++;
        for (int i = 8; i; --i)
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
    }
    return crc;
}

TempSensorData temp_sensor_read(void) {
    TempSensorData data = {0, 0, false};

    // Initialize on first call
    if (!initialized) {
        Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
        Wire.setClock(100000);
        initialized = true;
        Log_info("SHT40 I2C initialized (SDA=%d, SCL=%d)", I2C_SDA_PIN, I2C_SCL_PIN);
    }

    // Read sensor
    Wire.beginTransmission(TEMP_SENSOR_ADDR);
    Wire.write(SHT40_CMD_MEASURE);
    if (Wire.endTransmission() == 0) {
        delay(10);
        if (Wire.requestFrom((uint8_t)TEMP_SENSOR_ADDR, (uint8_t)6) == 6) {
            uint8_t raw[6];
            for (int i = 0; i < 6; i++) raw[i] = Wire.read();

            if (crc8(raw, 2) == raw[2] && crc8(raw + 3, 2) == raw[5]) {
                data.temperature = -45.0f + 175.0f * ((raw[0] << 8) | raw[1]) / 65535.0f;
                data.humidity = -6.0f + 125.0f * ((raw[3] << 8) | raw[4]) / 65535.0f;
                if (data.humidity < 0) data.humidity = 0;
                if (data.humidity > 100) data.humidity = 100;
                data.valid = true;
                Log_info("SHT40: %.2f°C, %.2f%%", data.temperature, data.humidity);
            } else {
                Log_error("SHT40 CRC error");
            }
        } else {
            Log_error("SHT40 read failed");
        }
    } else {
        Log_error("SHT40 not responding");
    }

    return data;
}

#else
TempSensorData temp_sensor_read(void) {
    return {0, 0, false};
}
#endif
