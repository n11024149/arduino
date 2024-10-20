#include <WiFi.h>
#include <ArduinoJson.h>

// WiFi 資訊
const char* ssid = "KC";                  // WiFi SSID
const char* password = "kc911211";        // WiFi 密碼

WiFiServer server(80);                    // 創建 Web 伺服器物件，使用 80 埠

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  // 等待 WiFi 連接
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("正在連接到 WiFi...");
  }
  Serial.println("WiFi 連接成功!");
  Serial.print("IP:");
  Serial.println(WiFi.localIP());

  server.begin();                         // 啟動伺服器
  Serial.println("HTTP 伺服器已啟動!");
}

void loop() {
  WiFiClient client = server.available(); // 檢查是否有客戶端連接

  if (client) { // 如果有客戶端連接
    Serial.println("新客戶端連接");
    String currentLine = ""; // 用於存儲每一行的內容
    while (client.connected()) {
      if (client.available()) {
        char c = client.read(); // 讀取數據
        Serial.write(c);        // 打印到串口
        currentLine += c;       // 將字符追加到當前行

        // 檢查是否收到請求的結尾
        if (c == '\n') {
          // 如果當前行為空，表示請求結束
          if (currentLine.length() == 0) {
            // 檢查是否是 POST 請求
            if (currentLine.startsWith("POST")) {
              String jsonData = "";
              while (client.available()) {
                jsonData += client.readString(); // 讀取請求體中的數據
              }

              Serial.println("接收到的 JSON 數據:");
              Serial.println(jsonData); // 打印接收到的 JSON 數據

              // 創建 JSON 文檔
              DynamicJsonDocument doc(1024);

              // 解析 JSON 數據
              DeserializationError error = deserializeJson(doc, jsonData);
              
              // 檢查解析是否成功
              if (error) {
                Serial.print(F("解析失敗: "));
                Serial.println(error.f_str());
                client.println("HTTP/1.1 400 Bad Request");
                client.println("Content-Type: application/json");
                client.println();
                client.println("{\"message\":\"解析失敗\"}");
                return;
              }

              // 提取特定欄位，忽略其他不必要欄位
              int id = doc["id"];                 // 獲取 id
              const char* name = doc["name"];     // 獲取 name

              // 打印提取的欄位
              Serial.print("ID: ");
              Serial.println(id);
              Serial.print("名稱: ");
              Serial.println(name);

              // 返回成功響應
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: application/json");
              client.println();
              client.println("{\"message\":\"成功接收\"}");
            }
            // 處理完請求後關閉客戶端連接
            break;
          } else {
            currentLine = ""; // 清空當前行
          }
        }
      }
    }
    client.stop(); // 關閉客戶端連接
    Serial.println("客戶端已斷開連接");
  }
}
