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
// Post
const char* serverName = "192.168.0.7";  // post IP
const int serverPort = 3001;
const char* severendpoint = "/data";
// Get
const char* externalApiHost = "192.168.0.7";  // get API
const int externalport = 3000;
const char* externalendpoint = "/products";
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

// 狀態模式
bool isDown = false;
bool isDetecting = false;
bool isCountdown = false;
bool isCountdownPaused = false;
bool hasStartedCountdown = false;
bool isUserVerified = false;  // 用戶是否已驗證

// 重試參數
const int MAX_RETRIES = 5;           // 最大重試次數 
const int RETRY_DELAY = 2000;        // 重試延遲(ms)
const int WIFI_CONNECT_TIMEOUT = 10;  // WiFi 連接超時次數(每次500ms)
const int DELETE_WAIT = 1000;        // 刪除後等待時間(ms)
const int SUCCESS_DISPLAY = 1000;     // 成功訊息顯示時間(ms)
const int ERROR_DISPLAY = 2000;       // 錯誤訊息顯示時間(ms)

// 按鈕控制相關
unsigned long buttonPressStartTime = 0;
const unsigned long LONG_PRESS_TIME = 1000; // 長按設定時長(ms)
bool isLongPress = false;
bool firstMotionDetected = false;  // 追蹤是否偵測到第一個動作

unsigned long lastSecondUpdate;
unsigned long countdownStart = 0;
unsigned long remainingTime = 0;
int countdownTime = 10;

String displayName = "";     // 顯示在 OLED
String userId = "";         // 回傳數據 user ID
const int jsonSize = 512;   // 宣告 Json 大小

// 動作列舉
enum Mode { PUSH_UP, SIT_UP, SQUAT };
Mode currentMode = PUSH_UP;

// Color set
const int redColor[3] = {255, 0, 0};
const int blueColor[3] = {0, 0, 255};
const int purpleColor[3] = {255, 0, 255};
const int yellowColor[3] = {255, 255, 0};
const int greenColor[3] = {0, 255, 0};

// Reset
void resetAllStates() {
    Serial.println("\n[Reset] Resetting all states...");
    
    // 清除所有狀態標誌
    isUserVerified = false;
    isDetecting = false;
    isCountdown = false;
    isCountdownPaused = false;
    hasStartedCountdown = false;
    firstMotionDetected = false;
    
    // 重置所有計數器
    count_push_up = 0;
    count_sit_up = 0;
    count_squat = 0;
    remainingTime = countdownTime;
    
    // 清除顯示
    displayText();
    setColor(0, 0, 0);
}

// set color
void setColor(int red, int green, int blue) {
  analogWrite(redPin, red);
  analogWrite(greenPin, green);
  analogWrite(bluePin, blue);
}

// Rgb Led Flash
void flashOnce(const int color[3]) {
  setColor(color[0], color[1], color[2]);
  delay(500);
  setColor(0, 0, 0);
}

// Oled
void displayText() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  // 顯示位置資訊
  u8g2.drawStr(0, 10, machine.c_str());
  
  if (!isUserVerified) {
    // 未配對時顯示等待資訊
    u8g2.drawStr(0, 35, "Waiting for user...");
  } else {
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
  }

  u8g2.sendBuffer();
}

// Get
void getExternalData() {
  if (isUserVerified) return;
  
  WiFiClient client;
  
  if (client.connect(externalApiHost, externalport)) {
    Serial.println("Connected to API server");
    
    client.print(String("GET ") + externalendpoint + " HTTP/1.1\r\n" +
                 "Host: " + externalApiHost + "\r\n" +
                 "Connection: close\r\n\r\n");
    
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println(">>> Client Timeout !");
        client.stop();
        flashOnce(redColor);
        return;
      }
    }
    
    String response = "";
    bool isBody = false;
    
    while (client.available()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        isBody = true;
        continue;
      }
      if (isBody) {
        response += line;
      }
    }
    
    Serial.println("\nReceived Response:");
    Serial.println(response);

    StaticJsonDocument<jsonSize> doc;
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
      flashOnce(redColor);
      client.stop();
      return;
    }

    // 檢查是否有 products 陣列
    JsonArray products;
    if (doc.containsKey("products")) {
      products = doc["products"].as<JsonArray>();
    } else {
      // 如果沒有 products 鍵，假設整個文檔就是陣列
      products = doc.as<JsonArray>();
    }

    // 檢查陣列是否為空
    if (products.size() == 0) {
      Serial.println("Empty JSON array");
      flashOnce(redColor);
      client.stop();
      return;
    }
    
    // 遍歷所有項目，尋找匹配的 machine
    bool found = false;
    for (JsonObject item : products) {
      if (!item.containsKey("machine") || !item.containsKey("username") || !item.containsKey("user")) {
        Serial.println("Missing required fields in object");
        continue; // 跳過這個項目，繼續檢查下一個
      }
      
      Serial.println("\nChecking item:");
      Serial.print("Machine: ");
      Serial.println(item["machine"].as<const char*>());
      Serial.print("Username: ");
      Serial.println(item["username"].as<const char*>());
      Serial.print("User ID: ");
      Serial.println(item["user"].as<const char*>());
      
      if (String(item["machine"].as<const char*>()) == machine) {
        displayName = item["username"].as<String>();
        userId = item["user"].as<String>();
        isUserVerified = true;
        found = true;
        
        Serial.println("User verified successfully!");
        displayText();
        flashOnce(greenColor);
        break;  // 找到匹配項目後退出循環
      }
    }
    
    if (!found) {
      Serial.println("No matching machine found");
      flashOnce(redColor);
    }
    
    client.stop();
  } else {
    Serial.println("Connection to API server failed");
    flashOnce(redColor);
  }
}

// Action
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

// Put
bool sendExerciseData() {
    int retryCount = 0;
    bool uploadSuccess = false;
    
    while (!uploadSuccess && retryCount < MAX_RETRIES) {
        // 檢查 WiFi 連接
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("\n[WiFi] Disconnected. Attempting to reconnect...");
            setColor(redColor[0], redColor[1], redColor[2]);
            WiFi.begin(ssid, password);
            
            int connectAttempts = 0;
            while (WiFi.status() != WL_CONNECTED && connectAttempts < WIFI_CONNECT_TIMEOUT) {
                delay(500);
                Serial.print(".");
                connectAttempts++;
            }
            
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[WiFi] Reconnection failed");
                retryCount++;
                flashOnce(redColor);
                continue;
            }
        }
        
        // 準備運動數據
        StaticJsonDocument<200> putData;
        putData["userID"] = userId;
        putData["machine"] = machine;
        
        String ActionType;
        int Frequency;
        
        switch (currentMode) {
            case PUSH_UP:
                ActionType = "Push Up";
                Frequency = count_push_up;
                break;
            case SIT_UP:
                ActionType = "Sit Up";
                Frequency = count_sit_up;
                break;
            case SQUAT:
                ActionType = "Squat";
                Frequency = count_squat;
                break;
        }
        
        putData["ActionType"] = ActionType;
        putData["Frequency"] = Frequency;
        
        String putJsonString;
        serializeJson(putData, putJsonString);
        
        Serial.println("\n[Upload] Sending data:");
        Serial.println(putJsonString);
        
        // 發送 PUT 請求
        setColor(purpleColor[0], purpleColor[1], purpleColor[2]); // 紫色表示處理中
        client.put(severendpoint, "application/json", putJsonString);
        
        int statusCode = client.responseStatusCode();
        String response = client.responseBody();
        
        Serial.print("[Upload] Status code: ");
        Serial.println(statusCode);
        Serial.print("[Upload] Response: ");
        Serial.println(response);
        
        if (statusCode == 200 && response.indexOf("ok") != -1) {  // 確保回應包含 "ok"
            Serial.println("[Upload] Success!");
            uploadSuccess = true;
            
            // 重置過程
            setColor(greenColor[0], greenColor[1], greenColor[2]);
            delay(SUCCESS_DISPLAY);
            
            // 完整的狀態重置
            resetAllStates();
            
            // 重新獲取用戶資料
            Serial.println("[Verify] Getting new user data...");
            getExternalData();
            
            return true;
        } else {
            Serial.println("[Upload] Failed or invalid response");
            flashOnce(redColor);
            retryCount++;
            
            if (retryCount < MAX_RETRIES) {
                Serial.print("[Retry] Waiting ");
                Serial.print(RETRY_DELAY / 1000);
                Serial.println(" seconds before next attempt...");
                delay(RETRY_DELAY);
            }
        }
    }
    
    if (!uploadSuccess) {
        Serial.println("\n[Error] Failed to upload after maximum retries");
        setColor(redColor[0], redColor[1], redColor[2]);
        delay(ERROR_DISPLAY);
        setColor(0, 0, 0);
    }
    
    return false;
}

// Setting
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
        if (!isDetecting) {
          isDetecting = true;
          isCountdown = false;
          isCountdownPaused = false;
          firstMotionDetected = false;
          setColor(blueColor[0], blueColor[1], blueColor[2]);
        } else {
          isCountdownPaused = !isCountdownPaused;
          if (isCountdownPaused) {
            setColor(redColor[0], redColor[1], redColor[2]);
          } else {
            setColor(blueColor[0], blueColor[1], blueColor[2]);
          }
        }
        displayText();
      }
    }
  }
  
  // 按鈕釋放
  if (buttonState == HIGH && lastButtonState == LOW) {
    if (isUserVerified && !isLongPress && !isDetecting) {
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

    if (remainingTime <= 0) {
      isCountdown = false;
      isDetecting = false;
      setColor(greenColor[0], greenColor[1], greenColor[2]);
      
      // 嘗試上傳數據直到成功
      bool uploadSuccess = false;
      while (!uploadSuccess) {
        uploadSuccess = sendExerciseData();
        if (!uploadSuccess) {
          delay(RETRY_DELAY);
        }
      }
      
      // 重置計時器狀態
      hasStartedCountdown = false;
      remainingTime = countdownTime;
      displayText();
    }
  }
}
