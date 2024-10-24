#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>

char ssid[] = "KC"; // WiFi SSID
char pass[] = "kc911211"; // WiFi 密碼
char server[] = "192.168.0.7"; // 目標伺服器
int port = 3000; // 伺服器端口

WiFiClient wifi;
HttpClient client = HttpClient(wifi, server, port);

void setup() {
    Serial.begin(9600);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
}

void loop() {
    String contentType = "application/json";
    String postData = "{\"Id\": 42,\"sensor\": \"temperature\",\"value\": 21.5,\"unit\": \"C\"}";

    client.post("/products", contentType, postData);

    int statusCode = client.responseStatusCode();
    Serial.print("Status code: ");
    Serial.println(statusCode);

    String response = client.responseBody();
    Serial.print("Response: ");
    Serial.println(response);

    delay(30000); // 每30秒發送一次
}