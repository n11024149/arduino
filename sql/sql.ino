#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>

const char* ssid = " WIFI 名字 ";
const char* password = " WIFI 密碼 ";

const char* serverName = " 樹莓派 IP "; 

String apiKeyValue = " API "; 
String sensorName = " Location ";     
String sensorLocation = " 裝置編號 ";  


unsigned long lastTime = 0;         
unsigned long timerDelay = 100;   

void setup() {
  Serial.begin(9600);  

  while (WiFi.begin(ssid, password) != WL_CONNECTED) {
    delay(1000);  
    Serial.println(" WiFi 在連了 不要吵...");
  }
  Serial.println(" WiFi 連到了啦");
}

void loop() {

  if ((millis() - lastTime) > timerDelay) {
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client; 
      HttpClient http(client, serverName, 80); 

      int randomValue1 = random(0, 1000);
      int randomValue2 = random(0, 1000);
      int randomValue3 = random(0, 1000);

      // 上傳的欄位跟數值
      String httpRequestData = "api_key=" + apiKeyValue  
                              + "&sensor=" + sensorName
                              + "&location=" + sensorLocation 
                              + "&value1=" + String(randomValue1) 
                              + "&value2=" + String(randomValue2) 
                              + "&value3=" + String(randomValue3);
      
      Serial.print("POST 數據: ");
      Serial.println(httpRequestData);
      
      http.beginRequest();
      http.post("/post-esp-data.php"); // post 用的 php
      http.sendHeader("Content-Type", "application/x-www-form-urlencoded");
      http.sendHeader("Content-Length", httpRequestData.length());
      http.print(httpRequestData);
      http.endRequest();

      int statusCode = http.responseStatusCode();
      String response = http.responseBody();

      Serial.print("HTTP : ");
      Serial.println(statusCode);
      Serial.print("伺服器: ");
      Serial.println(response);

      Serial.print("生成的隨機數: ");
      Serial.println(randomValue1);
      Serial.println(randomValue2);
      Serial.println(randomValue3);

    } else {
      Serial.println(" 你家的歪壞炸掉了 ");
    }
    lastTime = millis();
  }
}