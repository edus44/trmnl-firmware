#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

struct TempSensorData {
  float temperature;
  float humidity;
  bool valid;
};

/**
 * @brief Read temperature and humidity from sensor
 * Initializes sensor on first call, reads temperature/humidity.
 * @return TempSensorData with temperature, humidity and valid flag
 */
TempSensorData temp_sensor_read(void);

#endif // TEMP_SENSOR_H
