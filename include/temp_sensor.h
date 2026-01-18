#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

/**
 * @brief Read sensor and submit data via HTTP (single entry point)
 * Initializes sensor on first call, reads temperature/humidity, and POSTs to server.
 */
void temp_sensor_read_and_submit(void);

#endif // TEMP_SENSOR_H
