#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>

const char* ssid = "LAPTOP-H200NENE 7672";
const char* password = "11024211";

const char* serverName = "192.168.7.40"; 

String apiKeyValue = "lkjhgfdsa"; 
String sensorName = "Location";     
String sensorLocation = "001";  


unsigned long lastTime = 0;         
unsigned long timerDelay = 3000;   

void setup() {
  Serial.begin(9600);  

  while (WiFi.begin(ssid, password) != WL_CONNECTED) {
    delay(1000);  
    Serial.println("正在連接WiFi...");
  }
  Serial.println("已連接到 WiFi");
}

void loop() {

  if ((millis() - lastTime) > timerDelay) {
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client; 
      HttpClient http(client, serverName, 80); 

      int randomValue1 = random(0, 1000);
      int randomValue2 = random(0, 1000);
      int randomValue3 = random(0, 1000);

      String httpRequestData = "api_key=" + apiKeyValue  
                              + "&sensor=" + sensorName
                              + "&location=" + sensorLocation 
                              + "&value1=" + String(randomValue1) 
                              + "&value2=" + String(randomValue2) 
                              + "&value3=" + String(randomValue3);
      
      Serial.print("POST 數據: ");
      Serial.println(httpRequestData);
      
      http.beginRequest();
      http.post("/post-esp-data.php");
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
      Serial.println("WiFi 斷開連接");
    }
    lastTime = millis();
  }
}