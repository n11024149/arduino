#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <MPU6050_tockn.h>
#include <SPI.h>
#include <U8g2lib.h>

// wifi 資料參數設定區
const char* ssid = "C219-1";
const char* password = "CsieC219";
const char* serverName = "10.142.3.212";
const int serverPort = 80;
String apiKeyValue = "lkjhgfdsa";
String sensorName = "location";
String sensorLocation = "NHUgym-270F";
String user = "User 1";

// MPU6050, LED, OLED
MPU6050 mpu(Wire);
const int redPin = 5;
const int greenPin = 6;
const int bluePin = 7;
const int buttonPin = 2;
U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);

// 動作計數
int count_push_up = 0;
int count_sit_up = 0;
int count_squat = 0;

// 狀態模式參數
bool isDown = false;
bool isDetecting = false;
bool isCountdown = false;
bool isCountdownPaused = false;
bool hasStartedCountdown = false;
unsigned long lastSecondUpdate;
unsigned long countdownStart = 0;
unsigned long remainingTime = 0;
int countdownTime = 10;

enum Mode { PUSH_UP, SIT_UP, SQUAT };
Mode currentMode = PUSH_UP;

// 顏色
const int redColor[3] = {255, 0, 0};
const int blueColor[3] = {0, 0, 255};
const int purpleColor[3] = {255, 0, 255};
const int yellowColor[3] = {255, 255, 0};
const int greenColor[3] = {0, 255, 0};

// WiFi Server
WiFiServer server(serverPort);

// color
void setColor(int red, int green, int blue) {
  analogWrite(redPin, red);
  analogWrite(greenPin, green);
  analogWrite(bluePin, blue);
}

// LED
void flashOnce(const int color[3]) {
  setColor(color[0], color[1], color[2]);
  delay(500);
  setColor(0, 0, 0);
}

// post
void sendExerciseData() {
  // 保持原有發送資料的功能不變
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HttpClient http(client, serverName, 80);
    
    String httpRequestData = "api_key=" + apiKeyValue
                          + "&user=" + user
                          + "&location=" + sensorLocation
                          + "&value1=" + String(count_push_up)
                          + "&value2=" + String(count_sit_up)
                          + "&value3=" + String(count_squat);
    
    Serial.print("POST : ");
    Serial.println(httpRequestData);
    
    http.beginRequest();
    http.post("/post-esp-data.php");
    http.sendHeader("Content-Type", "application/x-www-form-urlencoded");
    http.sendHeader("Content-Length", httpRequestData.length());
    http.print(httpRequestData);
    http.endRequest();
    
    int statusCode = http.responseStatusCode();
    String response = http.responseBody();
    
    Serial.print("HTTP 狀態碼: ");
    Serial.println(statusCode);
    Serial.print("伺服器回應: ");
    Serial.println(response);
  }
}

// OLED
void displayText() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  // Location display
  u8g2.drawStr(0, 10, sensorLocation.c_str());
  
  // User display
  u8g2.drawStr(0, 25, user.c_str());

  // Exercise count display
  u8g2.setCursor(0, 40);
  switch (currentMode) {
    case PUSH_UP:
      u8g2.print("Push Up: ");
      u8g2.print(count_push_up);
      break;
    case SIT_UP:
      u8g2.print("Sit Up: ");
      u8g2.print(count_sit_up);
      break;
    case SQUAT:
      u8g2.print("Squat: ");
      u8g2.print(count_squat);
      break;
  }

  // Countdown display
  u8g2.setCursor(0, 55);
  if (isCountdown) {
    if (isCountdownPaused) {
      u8g2.print("PAUSED ");
    }
    u8g2.print("Time: ");
    u8g2.print(remainingTime);
    u8g2.print("s");
  } else if (remainingTime <= 0 && hasStartedCountdown) {
    u8g2.print("Exercise Complete!");
  }

  u8g2.sendBuffer();
}

// action
void detectMotion() {
  if (!isDetecting) return;
  
  // 如果處於暫停狀態，不進行計數
  if (isCountdownPaused) return;
  
  mpu.update();
 // 伏地
  if (currentMode == PUSH_UP) {
    if (mpu.getAngleX() > 30) {
      if (!isDown) {
        count_push_up++;
        isDown = true;
        Serial.println("伏地挺身次數: " + String(count_push_up));
        displayText();

        if (!hasStartedCountdown) {
          hasStartedCountdown = true;
          isCountdown = true;
          countdownStart = millis();
          remainingTime = countdownTime;
          lastSecondUpdate = countdownStart;
        }
      }
    } else if (mpu.getAngleX() < 10) {
      isDown = false;
    }
  }
 // 仰臥
  else if (currentMode == SIT_UP) {
    if (mpu.getAngleY() > 45) {
      if (!isDown) {
        count_sit_up++;
        isDown = true;
        Serial.println("仰臥起坐次數: " + String(count_sit_up));
        displayText();

        if (!hasStartedCountdown) {
          hasStartedCountdown = true;
          isCountdown = true;
          countdownStart = millis();
          remainingTime = countdownTime;
          lastSecondUpdate = countdownStart;
        }
      }
    } else if (mpu.getAngleY() < 20) {
      isDown = false;
    }
  }
 // 深蹲
  else if (currentMode == SQUAT) {
    if (mpu.getAngleY() > 30) {
      if (!isDown) {
        count_squat++;
        isDown = true;
        Serial.println("深蹲次數: " + String(count_squat));
        displayText();

        if (!hasStartedCountdown) {
          hasStartedCountdown = true;
          isCountdown = true;
          countdownStart = millis();
          remainingTime = countdownTime;
          lastSecondUpdate = countdownStart;
        }
      }
    } else if (mpu.getAngleY() < 10) {
      isDown = false;
    }
  }
}

// setting
void setup() {
  Serial.begin(115200);
  Wire.begin();
  mpu.begin();
  u8g2.begin();
  mpu.calcGyroOffsets(true);
  
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  // 檢查 WiFi 模組
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("無法連接到 WiFi 模組！");
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("請更新韌體 不更新也沒差");
  }

  // 連接 WiFi
  Serial.print("正在連接到 WiFi");
  while (WiFi.begin(ssid, password) != WL_CONNECTED) {
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
  
  Serial.println("陀螺儀已就緒");
  setColor(255, 0, 0);
  isDetecting = false;
}

// 燒香拜拜 不要出錯
void loop() {
  // 處理 WiFi 客戶端連接
  WiFiClient client = server.available();
  if (client) {
    Serial.println("新客戶端連接");
    String currentLine = "";
    String postData = "";
    bool isPost = false;
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        
        if (currentLine.indexOf("POST") == 0) {
          isPost = true;
        }
        
        if (c == '\n' && currentLine.length() == 0) {
          if (isPost) {
            delay(100);
            
            while (client.available()) {
              char c = client.read();
              postData += c;
            }
            
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, postData);
            
            if (!error) {
              const char* receivedId = doc["id"];
              const char* receivedName = doc["name"];
              
              Serial.println("收到的數據：");
              Serial.print("ID: ");
              Serial.println(receivedId);
              Serial.print("Name: ");
              Serial.println(receivedName);
              
              if (String(receivedId) == sensorLocation) {
                user = String(receivedName);
                Serial.println("ID 匹配成功！");
                Serial.print("更新後的 User 值: ");
                Serial.println(user);
                displayText(); // 更新顯示
              }
              
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: application/json");
              client.println("Connection: close");
              client.println();
              client.println("{\"status\":\"success\",\"message\":\"數據已處理\"}");
            } else {
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
    client.stop();
    Serial.println("客戶端斷開連接");
  }

  // 運動偵測邏輯
  static unsigned long buttonPressStartTime = 0;
  int buttonState = digitalRead(buttonPin);

  if (buttonState == LOW) {
    if (buttonPressStartTime == 0) {
      buttonPressStartTime = millis();
    }
  } else {
    if (buttonPressStartTime > 0) {
      if (millis() - buttonPressStartTime < 1000) {
        if (!isDetecting) {
          currentMode = (Mode)((currentMode + 1) % 3);
          Serial.print("切換動作: ");
          switch (currentMode) {
            case PUSH_UP: Serial.println("伏地挺身"); flashOnce(blueColor); break;
            case SIT_UP: Serial.println("仰臥起坐"); flashOnce(purpleColor); break;
            case SQUAT: Serial.println("深蹲"); flashOnce(yellowColor); break;
          }
          displayText();
        }
      } else {
        if (!isDetecting) {
          isDetecting = true;
          Serial.println("開始運動偵測");
          flashOnce(greenColor);
        } else if (isCountdown && !isCountdownPaused) {
          isCountdownPaused = true;
          Serial.println("暫停倒數");
          flashOnce(redColor);
        } else if (isCountdownPaused) {
          isCountdownPaused = false;
          Serial.println("繼續倒數");
          flashOnce(greenColor);
        }
      }
      buttonPressStartTime = 0;
    }
  }

  detectMotion();

  // 倒數計時邏輯
  if (isCountdown && !isCountdownPaused) {
    unsigned long currentTime = millis();
    if (currentTime - lastSecondUpdate >= 1000) {
      remainingTime--;
      lastSecondUpdate = currentTime;
      displayText();
      
      if (remainingTime <= 0) {
        Serial.println("倒數結束");
        isCountdown = false;
        isDetecting = false;
        
        Serial.println("開始上傳運動數據...");
        sendExerciseData();
        
        count_push_up = count_sit_up = count_squat = 0;
        setColor(0, 168, 0);
        displayText();
        delay(2000);
        hasStartedCountdown = false;
        displayText();
      }
    }
  }
}
