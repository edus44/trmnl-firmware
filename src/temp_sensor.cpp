#include "temp_sensor.h"

#if defined(BOARD_SEEED_RETERMINAL_E1001)

#include <Wire.h>
#include <trmnl_log.h>

// SHT40 constants
#define I2C_SDA_PIN        19
#define I2C_SCL_PIN        20
#define TEMP_SENSOR_ADDR   0x44
#define SHT40_CMD_MEASURE  0xFD  // High precision mode
#define SHT40_MEASURE_MS   9     // Max conversion time: 8.3ms + margin

// Inline CRC8 - allows better compiler optimization
static inline __attribute__((always_inline)) uint8_t crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0xFF;
    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 8; i; --i)
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
    }
    return crc;
}

TempSensorData temp_sensor_read(void) {
    TempSensorData data = {0.0f, 0.0f, false};

    // Init I2C - always needed after deep sleep (RAM is lost)
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000);  // 400kHz Fast Mode (SHT40 supports up to 1MHz)
    Log_info("SHT40 I2C initialized (SDA=%d, SCL=%d)", I2C_SDA_PIN, I2C_SCL_PIN);

    // Trigger measurement
    Wire.beginTransmission(TEMP_SENSOR_ADDR);
    Wire.write(SHT40_CMD_MEASURE);
    if (Wire.endTransmission() != 0) {
        Log_error("SHT40 not responding");
        Wire.end();
        return data;
    }

    delay(SHT40_MEASURE_MS);

    // Read 6 bytes: [temp_msb, temp_lsb, temp_crc, hum_msb, hum_lsb, hum_crc]
    if (Wire.requestFrom(TEMP_SENSOR_ADDR, 6) != 6) {
        Log_error("SHT40 read failed");
        Wire.end();
        return data;
    }

    uint8_t raw[6];
    for (uint8_t i = 0; i < 6; i++) {
        raw[i] = Wire.read();
    }

    // Release I2C before processing - reduces active peripheral time
    Wire.end();

    // Verify CRC for both temp and humidity
    if (crc8(raw, 2) != raw[2] || crc8(raw + 3, 2) != raw[5]) {
        Log_error("SHT40 CRC error");
        return data;
    }

    // Convert raw to physical values
    const uint16_t raw_temp = (raw[0] << 8) | raw[1];
    const uint16_t raw_hum  = (raw[3] << 8) | raw[4];

    data.temperature = -45.0f + 175.0f * (float)raw_temp / 65535.0f;
    float hum = -6.0f + 125.0f * (float)raw_hum / 65535.0f;

    // Clamp humidity [0, 100]
    data.humidity = (hum < 0.0f) ? 0.0f : (hum > 100.0f) ? 100.0f : hum;
    data.valid = true;

    Log_info("SHT40: %.2f°C, %.2f%%", data.temperature, data.humidity);

    return data;
}

#else
TempSensorData temp_sensor_read(void) {
    return {0, 0, false};
}
#endif
