#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <MPU6050_tockn.h>
#include <SPI.h>
#include <U8g2lib.h>

// WiFi 資料參數設定區
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
bool isDown = false;  // 是否處於下沉狀態
bool isDetecting = false;  // 是否正在偵測運動
bool isCountdown = false;  // 是否正在倒數
bool isCountdownPaused = false;  // 是否暫停倒數
bool hasStartedCountdown = false;  // 是否已經開始倒數
unsigned long lastSecondUpdate;  // 上次倒數更新的時間
unsigned long countdownStart = 0;  // 倒數開始時間
unsigned long remainingTime = 0;  // 剩餘時間
int countdownTime = 10;  // 倒數時間設定

// 動作模式枚舉
enum Mode { PUSH_UP, SIT_UP, SQUAT };
Mode currentMode = PUSH_UP;  // 當前的運動模式

// 顏色設定
const int redColor[3] = {255, 0, 0};
const int blueColor[3] = {0, 0, 255};
const int purpleColor[3] = {255, 0, 255};
const int yellowColor[3] = {255, 255, 0};
const int greenColor[3] = {0, 255, 0};

// WiFi 伺服器
WiFiServer server(serverPort);

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

// 上傳運動數據的函式
void sendExerciseData() {
  // 保持原有發送資料的功能不變
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HttpClient http(client, serverName, 80);
    
    // 構建 POST 請求數據
    String httpRequestData = "api_key=" + apiKeyValue
                          + "&user=" + user
                          + "&location=" + sensorLocation
                          + "&value1=" + String(count_push_up)
                          + "&value2=" + String(count_sit_up)
                          + "&value3=" + String(count_squat);
    
    Serial.print("POST : ");
    Serial.println(httpRequestData);
    
    // 發送 POST 請求
    http.beginRequest();
    http.post("/post-esp-data.php");
    http.sendHeader("Content-Type", "application/x-www-form-urlencoded");
    http.sendHeader("Content-Length", httpRequestData.length());
    http.print(httpRequestData);
    http.endRequest();
    
    // 取得伺服器回應
    int statusCode = http.responseStatusCode();
    String response = http.responseBody();
    
    Serial.print("HTTP 狀態碼: ");
    Serial.println(statusCode);
    Serial.print("伺服器回應: ");
    Serial.println(response);
  }
}

// 顯示文字到 OLED 的函式
void displayText() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  // 顯示位置資訊
  u8g2.drawStr(0, 10, sensorLocation.c_str());
  
  // 顯示使用者名稱
  u8g2.drawStr(0, 25, user.c_str());

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

// 初始化設定
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

  // 檢查韌體版本
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("請更新韌體 不更新也沒差");
  }

  // 連接到 WiFi
  Serial.print("正在連接到 WiFi");
  while (WiFi.begin(ssid, password) != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println(" 連接成功！");
  
  // 設定初始 LED 顏色
  setColor(0, 0, 0);

  // 設定初始顯示
  displayText();
}

// 主迴圈
void loop() {
  detectMotion();

  // 倒數計時處理
  if (isCountdown && !isCountdownPaused) {
    unsigned long currentMillis = millis();

    // 更新倒數時間
    if (currentMillis - lastSecondUpdate >= 1000) {
      remainingTime--;
      lastSecondUpdate = currentMillis;
      displayText();
    }

    // 倒數結束
    if (remainingTime <= 0) {
      isCountdown = false;
      setColor(greenColor[0], greenColor[1], greenColor[2]);
      delay(1000);  // 綠燈亮 1 秒
      flashOnce(greenColor);
      hasStartedCountdown = false;
      remainingTime = countdownTime;
      count_push_up = 0;
      count_sit_up = 0;
      count_squat = 0;
      displayText();
    }
  }
}
