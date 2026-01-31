# Firmware HTTP API Documentation

This document describes the HTTP communication flows between the firmware and the API server.

## Endpoint Summary

| Method | Endpoint       | Description                                      |
|:-------|:---------------|:-------------------------------------------------|
| GET    | `/api/setup`   | Initial registration and API credentials retrieval |
| GET    | `/api/display` | Retrieve configuration and image URL             |
| POST   | `/api/log`     | Submit device logs                               |
| GET    | (Image URL)    | Download the image to display                    |

---

## 1. Setup Flow (Registration)

This flow is executed to register the device and obtain the API Key.

**Endpoint:** `GET /api/setup`

### Headers Sent
| Header | Example Value | Description |
|:-------|:--------------|:------------|
| `ID` | `A0:B1:C2:D3:E4:F5` | Device MAC address |
| `Content-Type` | `application/json` | Content type |
| `FW-Version` | `1.0.0` | Current firmware version |
| `Model` | `TRMNL` | Device model |

### Expected Response (JSON)
```json
{
  "status": 200,
  "message": "Start setup",
  "api_key": "user-api-key",
  "friendly_id": "Device-1234",
  "image_url": "https://..."
}
```

---

## 2. Display Flow (Update)

This is the main flow executed periodically to check for new information to display.

**Endpoint:** `GET /api/display`

### Headers Sent
| Header | Example Value | Description |
|:-------|:--------------|:------------|
| `ID` | `A0:B1:C2:D3:E4:F5` | MAC Address |
| `Content-Type` | `application/json` | |
| `Access-Token` | `user-api-key` | API Key obtained in setup |
| `Refresh-Rate` | `900` | Configured sleep time (seconds) |
| `Battery-Voltage` | `3.85` | Battery voltage |
| `FW-Version` | `1.0.0` | Firmware version |
| `Model` | `TRMNL` | Model |
| `RSSI` | `-65` | WiFi signal strength |
| `temperature-profile`| `true` | Indicates temperature profile support |
| `Width` | `800` | Display width |
| `Height` | `480` | Display height |
| `Sensor-Temperature` | `24.5` | (Optional) Temperature if sensor present |
| `Sensor-Humidity` | `50.2` | (Optional) Humidity if sensor present |
| `special_function` | `true` | (Optional) If special function is active |

### Expected Response (JSON)
```json
{
  "status": 0,
  "image_url": "https://...",
  "image_url_timeout": 30,
  "filename": "image.bmp",
  "update_firmware": false,
  "maximum_compatibility": false,
  "firmware_url": "https://...",
  "refresh_rate": 900,
  "reset_firmware": false,
  "special_function": "none",
  "temperature_profile": "a", 
  "action": "none"
}
```
*Note: `temperature_profile` values ("a", "b", etc.) are mapped internally to numeric values.*

---

## 3. Log Submission Flow

The device sends accumulated logs to the server, including status metrics.

**Endpoint:** `POST /api/log`

### Headers Sent
| Header | Example Value | Description |
|:-------|:--------------|:------------|
| `ID` | `A0:B1:C2:D3:E4:F5` | MAC Address |
| `Accept` | `application/json, */*` | |
| `Access-Token` | `user-api-key` | |
| `Content-Type` | `application/json` | |

### Sent Body (JSON)
```json
{
  "logs": [
    {
      "created_at": 1700000000,
      "id": 123,
      "message": "Log message here",
      "source_line": 42,
      "source_path": "main.cpp",
      "wifi_signal": -60,
      "wifi_status": "Connected",
      "refresh_rate": 900,
      "sleep_duration": 900,
      "firmware_version": "1.0.0",
      "special_function": "",
      "battery_voltage": 3.9,
      "wake_reason": "Timer",
      "free_heap_size": 150000,
      "max_alloc_size": 140000,
      "retry": 1
    }
  ]
}
```

---

## 4. Image Download Flow

If `/api/display` returns an image URL, the firmware proceeds to download it.

**Endpoint:** URL obtained from the `image_url` field in the `/api/display` response.

**Method:** `GET`

### Headers Sent
| Header | Example Value | Description |
|:-------|:--------------|:------------|
| `Accept-Encoding` | `identity` | Disables compression for raw data |
| `ID` | `A0:B1:C2:D3:E4:F5` | (Only if hosted on same API domain) |
| `Access-Token` | `user-api-key` | (Only if hosted on same API domain) |
| `Content-Type` | (Automatic) | Headers collected from response |

### Behavior
- Detects redirects (307/308).
- Checks `Content-Type` to determine format (PNG/JPEG/BMP).
- Downloads binary image content.

### Image Format Requirements

The firmware supports downloading and displaying images with the following characteristics:

| Feature | Requirement | Notes |
|:---|:---|:---|
| **Supported Formats** | **BMP, PNG, JPEG** | Detected via `Content-Type` header (`image/png`, `image/jpeg`) or file signature (magic bytes `BM` for BMP). |
| **Max File Size** | **90,000 bytes** | (Approx 90KB). Images larger than this will be rejected with `HTTPS_IMAGE_FILE_TOO_BIG`. |
| **Pixel Format** | **1-bit** (Monochrome) | E-Paper displays are typically monochrome. The firmware decodes formatting accordingly. |
| **Resolution** | **800 x 480** | Native resolution for the display. |

*Note: For BMPs, 1-bit depth is expected. PNG/JPEG will be decoded to the display buffer.*

---

## 5. Error Handling

The communication layer implements specific strategies for handling network errors and timeouts.

### Connection Retries

If the AP connection or API request fails, the device performs a progressive retry mechanism before going to deep sleep. The retry counter is stored in persistent memory (`preferences`).

| Attempt | Sleep Duration (Before Retry) |
|:---|:---|
| 1st Retry | **15 seconds** |
| 2nd Retry | **30 seconds** |
| 3rd Retry | **60 seconds** |
| Max Retries | **900 seconds** (Default sleep time) |

*After the maximum number of retries is reached without success, the device goes to sleep for the standard duration (15 minutes by default) and resets the retry counter.*

### Timeouts

HTTP requests have defined timeout periods to prevent the device from hanging indefinitey.

- **Standard Request Timeout:** **15,000 ms (15 seconds)**. This applies to connection and data reception for API calls.
- **Image Download Timeout:**
    - Default: **15,000 ms**.
    - **Configurable:** The server can specify a custom timeout in the `/api/display` response using the `image_url_timeout` field (in seconds).
    - **Maximum:** The timeout is capped at `UINT16_MAX` milliseconds (approx **65 seconds**). If the server requests more, it is truncated and a warning log is generated.

### Error Actions

- **WiFi Weak/Disconnect:** If RSSI is too low or connection drops, the device shows a "WiFi Weak" or "WiFi Failed" error screen.
- **Format/Size Errors:** If the image is invalid or too big, an error screen (`MSG_FORMAT_ERROR` or `API_SIZE_ERROR`) is shown.
- **Deep Sleep on Error:** In most failure cases, to conserve battery, the device will display an error (if possible) and enter deep sleep immediately, attempting to recover on the next wake cycle.
