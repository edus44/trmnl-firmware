#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// Error codes for the HTTP utilities - using distinct values to avoid overlap
enum HttpError
{
  HTTPCLIENT_SUCCESS = 100,
  HTTPCLIENT_WIFICLIENT_ERROR = 101, // Failed to create client
  HTTPCLIENT_HTTPCLIENT_ERROR = 102  // Failed to connect
};

/**
 * @brief Log HTTP request details (method, URL, headers, body)
 */
inline void logHttpRequest(const char *method, const String &url, HTTPClient &https, const String &body = "")
{
  Log_info("========== HTTP REQUEST ==========");
  Log_info("Method: %s", method);
  Log_info("URL: %s", url.c_str());
  Log_info("--- Request Headers ---");
  // Note: HTTPClient doesn't expose headers after they're added, so we log them when adding
  if (body.length() > 0)
  {
    Log_info("--- Request Body ---");
    Log_info("%s", body.c_str());
  }
  Log_info("===================================");
}

/**
 * @brief Log HTTP response details (status code, headers, body)
 */
inline void logHttpResponse(int httpCode, HTTPClient &https, const String &responseBody = "")
{
  Log_info("========== HTTP RESPONSE ==========");
  Log_info("Status Code: %d (%s)", httpCode, https.errorToString(httpCode).c_str());
  Log_info("Content-Length: %d", https.getSize());
  Log_info("--- Response Headers ---");
  // Log collected headers
  int headerCount = https.headers();
  for (int i = 0; i < headerCount; i++)
  {
    Log_info("  %s: %s", https.headerName(i).c_str(), https.header(i).c_str());
  }
  if (responseBody.length() > 0)
  {
    Log_info("--- Response Body ---");
    // Truncate very long responses for logging
    if (responseBody.length() > 2000)
    {
      Log_info("%s... (truncated, total %d bytes)", responseBody.substring(0, 2000).c_str(), responseBody.length());
    }
    else
    {
      Log_info("%s", responseBody.c_str());
    }
  }
  Log_info("====================================");
}

/**
 * @brief Log headers being added to a request
 */
inline void logAddHeader(const String &name, const String &value)
{
  Log_info("  [Header] %s: %s", name.c_str(), value.c_str());
}

/**
 * @brief Higher-order function that sets up WiFiClient and HTTPClient, then runs a callback
 * @param url The initial URL to connect to
 * @param callback Function to call with the HTTPClient pointer and error code
 * @return The value returned by the callback
 */
template <typename Callback, typename ReturnType = decltype(std::declval<Callback>()(nullptr, (HttpError)0))>
ReturnType withHttp(const String &url, Callback callback)
{
  Log_info("==== withHttp() %s", url.c_str());

  bool isHttps = (url.indexOf("https://") != -1);

  // Conditionally allocate only the client we need
  WiFiClient *client = nullptr;

  if (isHttps)
  {
    WiFiClientSecure *secureClient = new WiFiClientSecure();
    secureClient->setInsecure();
    client = secureClient;
  }
  else
  {
    client = new WiFiClient();
  }

  // Check if client creation succeeded
  if (!client)
  {
    return callback(nullptr, HTTPCLIENT_WIFICLIENT_ERROR);
  }

  ReturnType result;
  { // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is

    HTTPClient https;
    if (https.begin(*client, url))
    {
      result = callback(&https, HTTPCLIENT_SUCCESS);
      https.end();
    }
    else
    {
      result = callback(nullptr, HTTPCLIENT_HTTPCLIENT_ERROR);
    }
  }
  delete client;

  return result;
}

#endif // HTTP_UTILS_H