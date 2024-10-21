#include <WiFiNINA.h>
#include <ArduinoJson.h>

// WiFi 資訊
const char* ssid = "C219-1";                  // WiFi SSID
const char* password = "CsieC219";        // WiFi 密碼
const int serverPort = 80;                // HTTP 伺服器埠號

// 用於比對的變量
const String sensorLocation = "NHUgym-270F";  // 用於比對的ID
String user = "User 1";                       // 用於存儲name的值

WiFiServer server(serverPort);

void setup() {
  Serial.begin(115200);
  
  // 檢查 WiFi 模組
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("無法連接到 WiFi 模組！");
    while (true); // 停止程序
  }

  // 檢查韌體版本
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("請更新韌體");
  }

  // 嘗試連接到 WiFi
  Serial.print("正在連接到 WiFi");
  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    status = WiFi.begin(ssid, password);
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nWiFi 連接成功！");

  // 打印網路資訊
  IPAddress ip = WiFi.localIP();
  Serial.print("IP 位址: ");
  Serial.println(ip);

  // 啟動伺服器
  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  
  if (client) {
    Serial.println("新客戶端連接");
    String currentLine = "";
    String postData = "";
    bool isPost = false;
    bool isBody = false;
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        
        // 檢查是否是 POST 請求
        if (currentLine.indexOf("POST") == 0) {
          isPost = true;
        }
        
        // 到達請求頭結束
        if (c == '\n' && currentLine.length() == 0) {
          if (isPost) {
            delay(100); // 等待 POST 數據
            
            // 讀取請求主體
            while (client.available()) {
              char c = client.read();
              postData += c;
            }
            
            // 解析 JSON
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, postData);
            
            if (!error) {
              // 獲取 JSON 中的值
              const char* receivedId = doc["id"];
              const char* receivedName = doc["name"];
              
              Serial.println("收到的數據：");
              Serial.print("ID: ");
              Serial.println(receivedId);
              Serial.print("Name: ");
              Serial.println(receivedName);
              
              // 比對 ID
              if (String(receivedId) == sensorLocation) {
                user = String(receivedName);
                Serial.println("ID 匹配成功！");
                Serial.print("更新後的 User 值: ");
                Serial.println(user);
              } else {
                Serial.println("ID 不匹配");
              }
              
              // 返回成功響應
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: application/json");
              client.println("Connection: close");
              client.println();
              client.println("{\"status\":\"success\",\"message\":\"數據已處理\"}");
            } else {
              // JSON 解析失敗響應
              client.println("HTTP/1.1 400 Bad Request");
              client.println("Content-Type: application/json");
              client.println("Connection: close");
              client.println();
              client.println("{\"status\":\"error\",\"message\":\"JSON解析失敗\"}");
            }
            break;
          }
        }
        
        if (c == '\n') {
          currentLine = "";
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    
    delay(1);
    client.stop();
    Serial.println("客戶端斷開連接");
  }
}
