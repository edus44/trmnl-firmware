#include <api-client/display.h>
#include <HTTPClient.h>
#include <trmnl_log.h>
#include <WiFiClientSecure.h>
#include <config.h>
#include <api_response_parsing.h>
#include <http_client.h>

void addHeaders(HTTPClient &https, ApiDisplayInputs &inputs)
{
  Log_info("--- Request Headers (Display API) ---");
  
  logAddHeader("ID", inputs.macAddress);
  https.addHeader("ID", inputs.macAddress);
  
  logAddHeader("Content-Type", "application/json");
  https.addHeader("Content-Type", "application/json");
  
  logAddHeader("Access-Token", inputs.apiKey);
  https.addHeader("Access-Token", inputs.apiKey);
  
  logAddHeader("Refresh-Rate", String(inputs.refreshRate));
  https.addHeader("Refresh-Rate", String(inputs.refreshRate));
  
  logAddHeader("Battery-Voltage", String(inputs.batteryVoltage));
  https.addHeader("Battery-Voltage", String(inputs.batteryVoltage));
  
  logAddHeader("FW-Version", inputs.firmwareVersion);
  https.addHeader("FW-Version", inputs.firmwareVersion);
  
  logAddHeader("Model", String(inputs.model));
  https.addHeader("Model", String(inputs.model));
  
  logAddHeader("RSSI", String(inputs.rssi));
  https.addHeader("RSSI", String(inputs.rssi));
  
  logAddHeader("temperature-profile", "true");
  https.addHeader("temperature-profile", "true");
  
  logAddHeader("Width", String(inputs.displayWidth));
  https.addHeader("Width", String(inputs.displayWidth));
  
  logAddHeader("Height", String(inputs.displayHeight));
  https.addHeader("Height", String(inputs.displayHeight));

  if (inputs.hasSensorData)
  {
    logAddHeader("Sensor-Temperature", String(inputs.temperature));
    https.addHeader("Sensor-Temperature", String(inputs.temperature));
    logAddHeader("Sensor-Humidity", String(inputs.humidity));
    https.addHeader("Sensor-Humidity", String(inputs.humidity));
  }

  if (inputs.specialFunction != SF_NONE)
  {
    logAddHeader("special_function", "true");
    https.addHeader("special_function", "true");
  }
}

ApiDisplayResult fetchApiDisplay(ApiDisplayInputs &apiDisplayInputs)
{

  return withHttp(
      apiDisplayInputs.baseUrl + "/api/display",
      [&apiDisplayInputs](HTTPClient *https, HttpError error) -> ApiDisplayResult
      {
        if (error == HttpError::HTTPCLIENT_WIFICLIENT_ERROR)
        {
          Log_error("Unable to create WiFiClient");
          return ApiDisplayResult{
              .error = https_request_err_e::HTTPS_UNABLE_TO_CONNECT,
              .response = {},
              .error_detail = "Unable to create WiFiClient",
          };
        }
        if (error == HttpError::HTTPCLIENT_HTTPCLIENT_ERROR)
        {
          Log_error("Unable to create HTTPClient");
          return ApiDisplayResult{
              .error = https_request_err_e::HTTPS_UNABLE_TO_CONNECT,
              .response = {},
              .error_detail = "Unable to create HTTPClient",
          };
        }

        https->setTimeout(15000);
        https->setConnectTimeout(15000);

        // Collect all response headers for logging
        const char *headerKeys[] = {"Content-Type", "Content-Length", "Location", "X-Request-Id", "Cache-Control", "Date", "Server"};
        https->collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(headerKeys[0]));

        String requestUrl = apiDisplayInputs.baseUrl + "/api/display";
        logHttpRequest("GET", requestUrl, *https);
        addHeaders(*https, apiDisplayInputs);

        delay(5);

        Log_info("Start location: %s", https->getLocation().c_str());
        int httpCode = https->GET();
        
        String payload = https->getString();
        logHttpResponse(httpCode, *https, payload);
        
        if(httpCode == HTTP_CODE_PERMANENT_REDIRECT ||httpCode == HTTP_CODE_TEMPORARY_REDIRECT){
              https->end();
              String redirectUrl = API_BASE_URL + https->getLocation();
              https->begin(redirectUrl);
              Log_info("Redirected to: %s", https->getLocation().c_str());
              https->setTimeout(15000);
              https->setConnectTimeout(15000);
              https->collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(headerKeys[0]));
              logHttpRequest("GET (redirect)", redirectUrl, *https);
              addHeaders(*https, apiDisplayInputs);
              httpCode = https->GET();
              payload = https->getString();
              logHttpResponse(httpCode, *https, payload);
            }

        if (httpCode < 0 ||
            !(httpCode == HTTP_CODE_OK ||
              httpCode == HTTP_CODE_MOVED_PERMANENTLY ||
              httpCode == HTTP_CODE_TOO_MANY_REQUESTS))
        {
          Log_error("[HTTPS] GET... failed, error: %s", https->errorToString(httpCode).c_str());

          return ApiDisplayResult{
              .error = https_request_err_e::HTTPS_RESPONSE_CODE_INVALID,
              .response = {},
              .error_detail = "HTTP Client failed with error: " + https->errorToString(httpCode) +
                              "(" + String(httpCode) + ")"};
        }

        // HTTP header has been send and Server response header has been handled
        Log_info("GET... code: %d", httpCode);

        size_t size = https->getSize();
        Log_info("Content size: %d", size);
        Log_info("Free heap size: %d", ESP.getMaxAllocHeap());

        auto apiResponse = parseResponse_apiDisplay(payload);

        if (apiResponse.outcome == ApiDisplayOutcome::DeserializationError)
        {
          return ApiDisplayResult{
              .error = https_request_err_e::HTTPS_JSON_PARSING_ERR,
              .response = {},
              .error_detail = "JSON parse failed with error: " +
                              apiResponse.error_detail};
        }
        else
        {
          return ApiDisplayResult{
              .error = https_request_err_e::HTTPS_NO_ERR,
              .response = apiResponse,
              .error_detail = ""};
        }
      });
}