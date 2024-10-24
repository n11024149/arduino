#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>

char ssid[] = "KC"; // WiFi SSID
char pass[] = "kc911211"; // WiFi 密碼
char server[] = "192.168.0.7"; // 目標伺服器
int port = 3000; // 伺服器端口

WiFiClient wifi;
HttpClient client = HttpClient(wifi, server, port);

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
}

void deleteLastRecord() {
    // 先發送GET請求獲取最新記錄
    client.get("/post");
    
    int statusCode = client.responseStatusCode();
    String response = client.responseBody();
    
    if (statusCode == 200) {
        // 從回應中提取id（這裡假設回應是單個對象）
        int idStart = response.indexOf("\"id\": \"") + 7;
        int idEnd = response.indexOf("\"", idStart);
        String id = response.substring(idStart, idEnd);
        
        // 使用獲取的id發送DELETE請求
        client.del("/post/" + id);
        
        statusCode = client.responseStatusCode();
        Serial.print("Delete status code: ");
        Serial.println(statusCode);
        
        response = client.responseBody();
        Serial.print("Delete response: ");
        Serial.println(response);
    }
    
    delay(1000); // 等待刪除完成
}

void loop() {
    // 先刪除上一筆記錄
    deleteLastRecord();
    
    // 準備新的資料
    String contentType = "application/json";
    String postData = String("{\"Id\": 42") + 
                     String(",\"sensor\": \"temperature\"") +
                     String(",\"value\": 21.5") +
                     String(",\"unit\": \"C\"}");

    // 發送新的資料
    client.post("/post", contentType, postData);

    int statusCode = client.responseStatusCode();
    Serial.print("Post status code: ");
    Serial.println(statusCode);

    String response = client.responseBody();
    Serial.print("Post response: ");
    Serial.println(response);

    delay(10000); // 每10秒發送一次
}
