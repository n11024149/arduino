#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <MPU6050_tockn.h>
#include <SPI.h>
#include <U8g2lib.h>

// WiFi and Server Settings
const char* ssid = "C219-1";
const char* password = "CsieC219";
const char* serverName = "10.142.3.212";
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

// Counter variables
int count_push_up = 0;
int count_sit_up = 0;
int count_squat = 0;

// State and mode variables
bool isDown = false;
bool isDetecting = false;
bool isCountdown = false;
bool isCountdownPaused = false;
bool hasStartedCountdown = false;
unsigned long lastSecondUpdate;
unsigned long countdownStart = 0;
unsigned long remainingTime = 0;
int countdownTime = 10; // 修改為60秒

enum Mode { PUSH_UP, SIT_UP, SQUAT };
Mode currentMode = PUSH_UP;

// Colors
const int redColor[3] = {255, 0, 0};
const int blueColor[3] = {0, 0, 255};
const int purpleColor[3] = {255, 0, 255};
const int yellowColor[3] = {255, 255, 0};
const int greenColor[3] = {0, 255, 0};

// Function to set RGB LED color
void setColor(int red, int green, int blue) {
  analogWrite(redPin, red);
  analogWrite(greenPin, green);
  analogWrite(bluePin, blue);
}

// RGB LED Flash
void flashOnce(const int color[3]) {
  setColor(color[0], color[1], color[2]);
  delay(500);
  setColor(0, 0, 0);
}

// Send data to server
void sendExerciseData() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HttpClient http(client, serverName, 80);
    
    String httpRequestData = "api_key=" + apiKeyValue
                          + "&sensor=" + user
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

// Setting dont touch
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

  while (WiFi.begin(ssid, password) != WL_CONNECTED) {
    Serial.println("WiFi...在連了 不要吵");
    delay(1000);
  }
  Serial.println("WiFi 連好了 滾去運動吧你");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  Serial.println("陀螺儀也好了 加油喔");
  setColor(255, 0, 0);
  
  isDetecting = false;
}

// Motion detection function
void detectMotion() {
  if (!isDetecting) return;
  mpu.update();

  // Push-up
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

  // Sit-up
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

  // Squat
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

// OLED display variables
void displayText() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  u8g2.drawStr(0, 10, sensorLocation.c_str());
  u8g2.drawStr(0, 30, user.c_str());

  // Action Switch
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

  // Time--&END
  if (isCountdown) {
    u8g2.setCursor(0, 64);
    if (isCountdownPaused) {
      u8g2.print("PAUSED - ");
    }
    u8g2.print("Time: ");
    u8g2.print(remainingTime);
  } else {
    if (remainingTime <= 0 && hasStartedCountdown) {
      u8g2.setCursor(0, 64);
      u8g2.print("------------END------------");
    }
  }

  u8g2.sendBuffer();
}

//燒香拜拜不要出錯
void loop() {
  static unsigned long buttonPressStartTime = 0;
  int buttonState = digitalRead(buttonPin);

  // Button logic
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

  // Countdown logic
  if (isCountdown && !isCountdownPaused) {
    unsigned long currentTime = millis();
    if (currentTime - lastSecondUpdate >= 1000) {
      remainingTime--;
      lastSecondUpdate = currentTime;
      displayText();
      
      // Countdown end
      if (remainingTime <= 0) {
        Serial.println("倒數結束");
        isCountdown = false;
        isDetecting = false;
        
        // 只在倒數結束時發送數據
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
