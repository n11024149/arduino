#include <WiFiNINA.h>  // 引用 WiFi 函式庫，適用於 Arduino Nano 33 IoT

const char* ssid = "LAPTOP-H200NENE 7672";
const char* password = "11024211";

void setup() {
  Serial.begin(115200);
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("\nConnected to WiFi");
  Serial.println(WiFi.localIP());
}

void sendHttpRequest(long rnd) {
  WiFiClient client;
  
  if (client.connect("jsonplaceholder.typicode.com", 80)) {
    
    client.print(String("GET ") + "/comments?id=" + String(rnd) + " HTTP/1.1\r\n" +
                 "Host: jsonplaceholder.typicode.com\r\n" +
                 "Connection: close\r\n\r\n");

    while (client.connected() || client.available()) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        Serial.println(line);
      }
    }

    client.stop();
  } else {
    Serial.println("Connection to server failed");
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    long rnd = random(1, 10);
    sendHttpRequest(rnd);
  } else {
    Serial.println("Connection lost");
  }

  delay(10000);
}
