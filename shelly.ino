#include <ArduinoJson.h>
#include <HTTPClient.h>

bool http_getpwr(String getstr, int * power)
{
  StaticJsonDocument<2000> doc;
  WiFiClient client;
  HTTPClient http;

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
        else 
        {
          *power = doc["total_power"];
          return true;
        }
      }
    } 
    else Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
  } 
  else Serial.printf("[HTTP} Unable to connect\n"); 
  return false;
}
