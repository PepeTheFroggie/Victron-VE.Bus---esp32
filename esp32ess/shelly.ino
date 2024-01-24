#include <ArduinoJson.h>
#include <HTTPClient.h>

int http_getpwr(String getstr)
{
  StaticJsonDocument<2000> doc;
  WiFiClient client;
  HTTPClient http;
  int pwr = 12345;

  if (http.begin(client, getstr)) 
  {  
    int httpCode = http.GET();
    if (httpCode > 0) 
    {
      if (httpCode == HTTP_CODE_OK) 
      {
        String payload = http.getString();
        //Serial.println(payload);
        DeserializationError error = deserializeJson(doc, payload);
        if (error) 
        {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
        }
        else pwr = doc["total_power"];
      }
    } 
    else Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
  } 
  else Serial.printf("[HTTP} Unable to connect\n"); 
  return pwr;
}
