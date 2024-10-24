#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <MPU6050_tockn.h>
#include <SPI.h>
#include <U8g2lib.h>

// WiFi 資料參數設定區
const char* ssid = "KC";
const char* password = "kc911211";
const char* serverName = "192.168.0.7";  // post IP
const int serverPort = 3000;
const char* externalApiHost = "192.168.0.7";  // get API
const int externalport = 3000;
const char *externalendpoint = "/products";
// String apiKeyValue = "lkjhgfdsa"; // 樹梅
// String sensorName = "location"; // 樹梅
String machine = "NHUgym-270F";

WiFiClient wifi;
HttpClient client = HttpClient(wifi, serverName, serverPort);

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
bool isUserVerified = false;  // 用於追踪用戶是否已驗證

// 按鈕控制相關變數
unsigned long buttonPressStartTime = 0;
const unsigned long LONG_PRESS_TIME = 1000; // 長按閾值為1秒
bool isLongPress = false;
bool firstMotionDetected = false;  // 追踪是否偵測到第一個動作

unsigned long lastSecondUpdate;
unsigned long countdownStart = 0;
unsigned long remainingTime = 0;
int countdownTime = 10;

String displayName = "";     // 用於顯示在 OLED 上的 username
String userId = "";         // 用於回傳數據的 user ID
const int jsonSize = 512;   // 增加 JSON 文檔大小以確保足夠空間

// 動作模式枚舉
enum Mode { PUSH_UP, SIT_UP, SQUAT };
Mode currentMode = PUSH_UP;

// 顏色設定
const int redColor[3] = {255, 0, 0};
const int blueColor[3] = {0, 0, 255};
const int purpleColor[3] = {255, 0, 255};
const int yellowColor[3] = {255, 255, 0};
const int greenColor[3] = {0, 255, 0};

// 設定 LED 顏色
void setColor(int red, int green, int blue) {
  analogWrite(redPin, red);
  analogWrite(greenPin, green);
  analogWrite(bluePin, blue);
}

// LED 閃爍一次
void flashOnce(const int color[3]) {
  setColor(color[0], color[1], color[2]);
  delay(500);
  setColor(0, 0, 0);
}

// 顯示文字到 OLED 的函式
void displayText() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  // 顯示位置資訊
  u8g2.drawStr(0, 10, machine.c_str());
  
  // 顯示用戶名
  u8g2.drawStr(0, 25, displayName.c_str());

  // 顯示運動次數
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

  // 顯示倒數計時
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

// 從外部 API 獲取數據
void getExternalData() {
  if (isUserVerified) return;
  
  WiFiClient client;
  
  if (client.connect(externalApiHost, externalport)) {
    Serial.println("Connected to API server");
    
    // 發送 HTTP GET 請求
    client.print(String("GET ") + externalendpoint + " HTTP/1.1\r\n" +
                 "Host: " + externalApiHost + "\r\n" +
                 "Connection: close\r\n\r\n");
    
    // 等待伺服器回應
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println(">>> Client Timeout !");
        client.stop();
        flashOnce(redColor);
        return;
      }
    }
    
    // 讀取完整回應
    String response = "";
    bool isBody = false;
    
    while (client.available()) {
      String line = client.readStringUntil('\n');
      
      // 當遇到空行時，表示 headers 結束，接下來是 body
      if (line == "\r") {
        isBody = true;
        continue;
      }
      
      // 只儲存 body 部分
      if (isBody) {
        response += line;
      }
    }
    
    Serial.println("\nReceived Response:");
    Serial.println(response);
    
    // 解析 JSON 數組
    StaticJsonDocument<jsonSize> doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
      flashOnce(redColor);
      client.stop();
      return;
    }
    
    // 檢查是否為空數組
    if (doc.size() == 0) {
      Serial.println("Empty JSON array");
      flashOnce(redColor);
      client.stop();
      return;
    }
    
    // 獲取第一個對象
    JsonObject item = doc[0];
    
    // 檢查必要欄位是否存在
    if (!item.containsKey("id") || !item.containsKey("username") || !item.containsKey("user")) {
      Serial.println("Missing required fields in object");
      flashOnce(redColor);
      client.stop();
      return;
    }
    
    // 打印解析後的數據
    Serial.println("\nParsed data:");
    Serial.print("Received ID: ");
    Serial.println(item["id"].as<const char*>());
    Serial.print("Expected ID: ");
    Serial.println(machine);
    Serial.print("Username: ");
    Serial.println(item["username"].as<const char*>());
    Serial.print("User ID: ");
    Serial.println(item["user"].as<const char*>());
    
    // 比對 ID
    if (String(item["id"].as<const char*>()) == machine) {
      displayName = item["username"].as<String>();
      userId = item["user"].as<String>();
      isUserVerified = true;
      
      Serial.println("User verified successfully!");
      Serial.print("Display Name: ");
      Serial.println(displayName);
      Serial.print("User ID: ");
      Serial.println(userId);
      
      displayText();
      flashOnce(greenColor);
    } else {
      Serial.println("Location mismatch! Verification failed.");
      Serial.print("Received ID: '");
      Serial.print(item["id"].as<const char*>());
      Serial.print("', Expected: '");
      Serial.print(machine);
      Serial.println("'");
      flashOnce(redColor);
    }
    
    client.stop();
  } else {
    Serial.println("Connection to API server failed");
    flashOnce(redColor);
  }
}

// 偵測運動動作的函式
void detectMotion() {
  if (!isDetecting) return;
  
  // 如果處於暫停狀態，不進行計數
  if (isCountdownPaused) return;
  
  mpu.update();
  
  // 伏地挺身
  if (currentMode == PUSH_UP) {
    if (mpu.getAngleX() > 30) {
      if (!isDown) {
        count_push_up++;
        isDown = true;
        Serial.println("伏地挺身次數: " + String(count_push_up));
        displayText();

        // 倒數計時尚未開始，則啟動
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
  
  // 仰臥起坐
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

// POST
void sendExerciseData() {
  if (WiFi.status() == WL_CONNECTED) {
    // 先刪除上一筆記錄
    deleteLastRecord();
    
    // 準備 JSON 數據
    String contentType = "application/json";
    String postData = "{""\"使用者ID\":\"" + userId + "\","
                     "\"裝置名稱\":\"" + machine + "\","
                     "\"伏地挺身\":" + String(count_push_up) + ","
                     "\"仰臥起坐\":" + String(count_sit_up) + ","
                     "\"深蹲\":" + String(count_squat) + "}";
    
    // 發送 POST 請求
    client.post("/post", contentType, postData);
    
    int statusCode = client.responseStatusCode();
    String response = client.responseBody();
    
    Serial.print("POST Status code: ");
    Serial.println(statusCode);
    Serial.print("Response: ");
    Serial.println(response);
  }
}

// 更新POST記錄
void deleteLastRecord() {
    // 發送 GET 請求獲取最新記錄
    client.get("/post");
    
    int statusCode = client.responseStatusCode();
    String response = client.responseBody();
    
    if (statusCode == 200) {
        // 從回應中提取 id
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, response);
        
        if (!error) {
            // 假設回應是個數組且我們要最後一個記錄
            String id = doc[0]["id"].as<String>();
            
            // 使用獲取的 id 發送 DELETE 請求
            client.del("/post/" + id);
            
            statusCode = client.responseStatusCode();
            Serial.print("Delete status code: ");
            Serial.println(statusCode);
            
            response = client.responseBody();
            Serial.print("Delete response: ");
            Serial.println(response);
        }
    }
    
    delay(1000); // 等待刪除完成
}

// 設定
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

  // 初始化狀態
  isUserVerified = false;
  isDetecting = false;
  isCountdown = false;
  isCountdownPaused = false;
  hasStartedCountdown = false;

  // 連接 WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nConnected to WiFi");
  Serial.println(WiFi.localIP());
  
  setColor(0, 0, 0);
  displayText();
}

// 不要亂動喔 你最好保證他沒事 不然就是你要出事
void loop() {
  static unsigned long lastGetRequest = 0;
  unsigned long currentMillis = millis();

  // 按鈕控制邏輯
  static bool lastButtonState = HIGH;
  bool buttonState = digitalRead(buttonPin);
  
  // 如果用戶未驗證，每5秒嘗試獲取一次數據
  if (!isUserVerified && currentMillis - lastGetRequest >= 5000) {
    getExternalData();
    lastGetRequest = currentMillis;
  }

  // 按鈕按下
  if (buttonState == LOW && lastButtonState == HIGH) {
    buttonPressStartTime = currentMillis;
    isLongPress = false;
  }
  
  // 按鈕持續按住
  if (buttonState == LOW) {
    if (!isLongPress && (currentMillis - buttonPressStartTime >= LONG_PRESS_TIME)) {
      isLongPress = true;
      if (isUserVerified) {
        // 長按處理：控制當前選擇的運動模式的偵測開始/暫停
        if (!isDetecting) {
          // 開始運動偵測，但不立即開始倒數計時
          isDetecting = true;
          isCountdown = false;  // 等待第一個動作再開始倒數
          isCountdownPaused = false;
          firstMotionDetected = false;  // 重置第一次動作偵測狀態
          setColor(blueColor[0], blueColor[1], blueColor[2]); // 藍色表示準備開始
        } else {
          // 暫停/繼續當前運動的偵測
          isCountdownPaused = !isCountdownPaused;
          if (isCountdownPaused) {
            setColor(redColor[0], redColor[1], redColor[2]); // 紅色表示暫停
          } else {
            setColor(blueColor[0], blueColor[1], blueColor[2]); // 藍色表示繼續
          }
        }
        displayText();
      }
    }
  }
  
  // 按鈕釋放 - 處理短按切換模式
  if (buttonState == HIGH && lastButtonState == LOW) {
    if (isUserVerified && !isLongPress && !isDetecting) {  // 只有在未開始偵測時才能切換模式
      // 切換運動模式
      switch (currentMode) {
        case PUSH_UP:
          currentMode = SIT_UP;
          flashOnce(purpleColor);
          break;
        case SIT_UP:
          currentMode = SQUAT;
          flashOnce(yellowColor);
          break;
        case SQUAT:
          currentMode = PUSH_UP;
          flashOnce(blueColor);
          break;
      }
      displayText();
    }
  }
  
  lastButtonState = buttonState;

  // 只有在用戶驗證後且開始偵測時才進行動作偵測
  if (isUserVerified && isDetecting && !isCountdownPaused) {
    detectMotion();
  }

  // 倒數計時處理
  if (isCountdown && !isCountdownPaused) {
    if (currentMillis - lastSecondUpdate >= 1000) {
      remainingTime--;
      lastSecondUpdate = currentMillis;
      displayText();
    }

    // 倒數結束
    if (remainingTime <= 0) {
      isCountdown = false;
      isDetecting = false;
      firstMotionDetected = false;  // 重置第一次動作偵測狀態
      setColor(greenColor[0], greenColor[1], greenColor[2]);
      delay(1000);
      flashOnce(greenColor);
      
      sendExerciseData();
      
      hasStartedCountdown = false;
      remainingTime = countdownTime;
      count_push_up = 0;
      count_sit_up = 0;
      count_squat = 0;
      displayText();
    }
  }
}
