#include <WiFiNINA.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <MPU6050_tockn.h>
#include <SPI.h>
#include <U8g2lib.h>

// WiFi 固定 IP
const char *ssid = "KC";
const char *password = "kc911211";

WiFiServer server(80);  // 開啟伺服器在80端口
WiFiClient client;

// MPU6050, LED, OLED
MPU6050 mpu(Wire);
const int redPin = 5;
const int greenPin = 6;
const int bluePin = 7;
const int buttonPin = 2;
U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);

// 計數變量
int count_push_up = 0;
int count_sit_up = 0;
int count_squat = 0;

// 狀態與模式
bool isDown = false;
bool isDetecting = false;
bool isCountdown = false;
bool isCountdownPaused = false;
bool hasStartedCountdown = false;
bool isUserAuthenticated = false;  // 確保接收到正確的 JSON 後，才允許進行動作偵測
bool buttonState = HIGH;       // 儲存當前按鈕狀態
bool lastButtonState = HIGH;   // 儲存上次按鈕狀態
unsigned long lastDebounceTime = 0;  // 防抖時間
unsigned long debounceDelay = 50;    // 防抖延遲時間
unsigned long lastSecondUpdate;
unsigned long countdownStart = 0;
unsigned long remainingTime = 0;
int countdownTime = 10;  // 60秒倒數(10秒測試版)

enum Mode { PUSH_UP, SIT_UP, SQUAT };
Mode currentMode = PUSH_UP;

// 顏色
const int redColor[3] = {255, 0, 0};
const int blueColor[3] = {0, 0, 255};
const int purpleColor[3] = {255, 0, 255};
const int yellowColor[3] = {255, 255, 0};
const int greenColor[3] = {0, 255, 0};

// OLED 顯示變數區
String Sensor_ID = "NHUgym-270F";  // Arduino 的 Sensor ID
String user = "User 1";

// 長按設定
const int longPressDuration = 1000;  // 長按的閾值時間（毫秒）
unsigned long buttonPressStartTime = 0;  // 按鈕按下的開始時間
bool isLongPress = false;  // 用於標記是否是長按

// RGB LED 設定
void setColor(int red, int green, int blue) {
  analogWrite(redPin, red);
  analogWrite(greenPin, green);
  analogWrite(bluePin, blue);
}

// RGB LED 閃爍
void flashOnce(const int color[3]) {
  setColor(color[0], color[1], color[2]);
  delay(500);
  setColor(0, 0, 0);
}

// 設定 不要動
void setup() {
  Serial.begin(115200);
  Wire.begin();
  mpu.begin();  // MPU-6050
  u8g2.begin();  // OLED
  mpu.calcGyroOffsets(true);
  
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  while (WiFi.begin(ssid, password) != WL_CONNECTED) {
    Serial.println(" WiFi 在連了 不要催我 ");
    delay(1000);
  }
  Serial.println(" WiFi 連接成功了 去運動吧你 ");
  Serial.print("IP:");
  Serial.println(WiFi.localIP());

  server.begin();  // 啟動伺服器
  Serial.println("HTTP 伺服器已啟動!");

  setColor(4, 0, 0);  // 初始紅燈亮起

  // 初始不偵測 等待選擇動作
  isDetecting = false;
}

// 動作偵測
void detectMotion() {
  if (!isDetecting) return;  // 如未啟用 則返回
  mpu.update();

  // 伏地挺身
  if (currentMode == PUSH_UP) {
    if (mpu.getAngleX() > 30) {
      if (!isDown) {
        count_push_up++;
        isDown = true;
        Serial.println("伏地挺身次數: " + String(count_push_up));
        displayText();
        startCountdownIfNeeded();
      }
    } else if (mpu.getAngleX() < 10) {
      isDown = false;
    }
  }
  // 仰臥起坐
  else if (currentMode == SIT_UP) {
    if (mpu.getAngleY() > 45) {
      if (!isDown) {
        count_sit_up++;
        isDown = true;
        Serial.println("仰臥起坐次數: " + String(count_sit_up));
        displayText();
        startCountdownIfNeeded();
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
        startCountdownIfNeeded();
      }
    } else if (mpu.getAngleY() < 10) {
      isDown = false;
    }
  }
}

// 開始倒數
void startCountdownIfNeeded() {
  if (!hasStartedCountdown) {
    hasStartedCountdown = true;
    isCountdown = true;
    countdownStart = millis();
    remainingTime = countdownTime;
    lastSecondUpdate = countdownStart;
  }
}

// OLED 顯示
void displayText() {
  u8g2.clearBuffer(); 
  u8g2.setFont(u8g2_font_ncenB08_tr);

  // 顯示裝置名稱和使用者
  u8g2.drawStr(0, 10, Sensor_ID.c_str());
  u8g2.drawStr(0, 30, user.c_str());

  // 顯示當前動作 次數
  u8g2.setCursor(0, 50);
  switch (currentMode) {
    case PUSH_UP:
      u8g2.print("Push Up : ");
      u8g2.print(count_push_up);
      break;
    case SIT_UP:
      u8g2.print("Sit Up : ");
      u8g2.print(count_sit_up);
      break;
    case SQUAT:
      u8g2.print("Squat : ");
      u8g2.print(count_squat);
      break;
  }

  // 倒數秒數顯示
  if (isCountdown) {
    if (millis() - lastSecondUpdate >= 1000) {
      remainingTime--;
      lastSecondUpdate = millis();
    }
    u8g2.setCursor(0, 60);
    u8g2.print("Countdown: ");
    u8g2.print(remainingTime);
  }

  u8g2.sendBuffer();
}

void loop() {
  // 檢查按鈕狀態
  int reading = digitalRead(buttonPin);

  // 檢查按鈕狀態以進行防抖
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  // 防抖
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      // 按鈕被按下時，執行操作
      if (buttonState == LOW) {
        buttonPressStartTime = millis();
        isLongPress = false; // 標記為不是長按
      }
    }

    // 檢查長按
    if (buttonState == LOW && !isLongPress) {
      if (millis() - buttonPressStartTime >= longPressDuration) {
        isLongPress = true;
        Serial.println("長按偵測到，停止運動");
        isDetecting = false; // 停止偵測
        hasStartedCountdown = false; // 重置倒數
        user = "用戶名"; // 重置使用者名
        flashOnce(redColor); // 重新設置顏色
      }
    }
  }

  // 更新按鈕狀態
  lastButtonState = reading;

  // 檢查伺服器是否有客戶端連接
  client = server.available(); // 監聽客戶端

  if (client) {
    String currentLine = ""; // 儲存當前行
    while (client.connected()) {
      if (client.available()) {
        char c = client.read(); // 讀取一個字元
        Serial.write(c); // 打印到序列監視器

        // 如果該字元是換行符，則該行結束
        if (c == '\n') {
          // 如果當前行是空行，則發送回應
          if (currentLine.length() == 0) {
            // 發送HTTP回應頭
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: application/json");
            client.println("Connection: close");
            client.println();

            // 處理POST請求
            // 注意：這裡需要根據具體的 JSON 格式進行解析
            if (currentLine.startsWith("POST")) {
              String jsonPayload = ""; // 儲存 JSON 負載
              while (client.available()) {
                char c = client.read();
                jsonPayload += c;
              }

              Serial.println("接收到的JSON:");
              Serial.println(jsonPayload);

              // 解析 JSON
              DynamicJsonDocument doc(1024);
              DeserializationError error = deserializeJson(doc, jsonPayload);

              if (!error) {
                // 提取 JSON 中的用戶名等信息
                String id = doc["id"]; // 假設這是 JSON 中的 ID
                String name = doc["name"]; // 假設這是 JSON 中的 name

                // 如果 ID 與 Sensor_ID 匹配，則更新 user
                if (id == Sensor_ID) {
                  user = name; // 更新 user 的值
                  Serial.println("用戶名稱已更新: " + user);
                  isUserAuthenticated = true; // 設置用戶認證為真
                }
              } else {
                Serial.print(F("JSON解析失敗: "));
                Serial.println(error.f_str());
              }
            }

            // 結束客戶端連接
            break;
          } else { // 重置當前行
            currentLine = "";
          }
        } else if (c != '\r') { // 忽略回車字符
          currentLine += c; // 加入到當前行
        }
      }
    }
    // 關閉客戶端連接
    client.stop();
    Serial.println("客戶端已斷開連接");
  }

  // 進行動作偵測
  detectMotion();
}
