#include <WiFiNINA.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <MPU6050_tockn.h>
#include <SPI.h>
#include <U8g2lib.h>

// WiFi 固定IP配置
const char *ssid = "KC";
const char *password = "kc911211";
IPAddress localIp(192, 168, 1, 100); // Local IP
IPAddress gateway(192, 168, 1, 1); // 閘道
IPAddress subnet(255, 255, 255, 0); // 遮罩

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
bool isCountdownPaused = false; // 倒計時是否暫停
bool hasStartedCountdown = false; // 是否已開始倒計時
unsigned long lastSecondUpdate;    // 用于追踪最后更新时间
unsigned long countdownStart = 0;
unsigned long remainingTime = 0; // 剩餘倒計時時間
int countdownTime = 10; // 60秒倒計時

enum Mode { PUSH_UP, SIT_UP, SQUAT };
Mode currentMode = PUSH_UP;

// 顏色
const int redColor[3] = {255, 0, 0};
const int blueColor[3] = {0, 0, 255};
const int purpleColor[3] = {255, 0, 255};
const int yellowColor[3] = {255, 255, 0};
const int greenColor[3] = {0, 255, 0};

// OLED 顯示變數區
String Sensor_ID = "NHUgym-270F";
String user = "User 1";

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
  mpu.begin(); // MPU-6050
  u8g2.begin();  // OLED
  mpu.calcGyroOffsets(true);
  
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  WiFi.config(localIp, gateway, subnet);
  while (WiFi.begin(ssid, password) != WL_CONNECTED) {
    Serial.println(" WiFi 在連了 不要催我 ");
    delay(1000);
  } 
  Serial.println(" WiFi 連接成功了 去運動吧你 ");
  Serial.print("IP:");
  Serial.println(WiFi.localIP());

  Serial.println("MPU6050 準備好了");
  setColor(4, 0, 0); // 初始紅燈亮起

  // 一開始不進行偵測，等待用戶選擇動作
  isDetecting = false;
}

// 動作偵測
void detectMotion() {
  if (!isDetecting) return;  // 如果未啟用偵測，直接返回
  mpu.update();

  // 伏地挺身
  if (currentMode == PUSH_UP) {
    if (mpu.getAngleX() > 30) {
      if (!isDown) {
        count_push_up++;
        isDown = true;
        Serial.println("伏地挺身次數: " + String(count_push_up));
        displayText();

        // 檢查是否需要開始倒計時
        if (!hasStartedCountdown) {
          hasStartedCountdown = true; // 設置已開始倒計時
          isCountdown = true; // 開始倒計時
          countdownStart = millis(); // 設定倒計時開始時間
          remainingTime = countdownTime; // 設定剩餘時間為60秒
          lastSecondUpdate = countdownStart; // 更新最後一次計時時間
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

        // 檢查是否需要開始倒計時
        if (!hasStartedCountdown) {
          hasStartedCountdown = true; // 設置已開始倒計時
          isCountdown = true; // 開始倒計時
          countdownStart = millis(); // 設定倒計時開始時間
          remainingTime = countdownTime; // 設定剩餘時間為60秒
          lastSecondUpdate = countdownStart; // 更新最後一次計時時間
        }
      }
    } else if (mpu.getAngleY() < 20) {
      isDown = false;
    }
  }

  // 深蹲
else if (currentMode == SQUAT) {
  float angleY = mpu.getAngleY(); // 獲取Z軸角度
  Serial.print("Z軸角度: ");
  Serial.println(angleY); // 輸出當前Z軸角度，方便調試

  if (angleY > 30) {  // 可嘗試調整這裡的角度範圍
    if (!isDown) {
      count_squat++;
      isDown = true;
      Serial.println("深蹲次數: " + String(count_squat));
      displayText();

      // 檢查是否需要開始倒計時
      if (!hasStartedCountdown) {
        hasStartedCountdown = true;
        isCountdown = true;
        countdownStart = millis();
        remainingTime = countdownTime;
        lastSecondUpdate = countdownStart;
      }
    }
  } else if (angleY < 10) { // 調整這裡的範圍以符合動作返回原始狀態的角度
    isDown = false;
  }
}

}

// 开始倒计时
void startCountdown() {
  if (!isCountdown) {
    isCountdown = true; // 开始倒计时
    countdownStart = millis(); // 设置倒计时开始时间
    remainingTime = countdownTime; // 设置剩余时间为60秒
  }
}

// OLED 顯示
void displayText() {
  u8g2.clearBuffer(); 
  u8g2.setFont(u8g2_font_ncenB08_tr); 

  // 顯示裝置名稱和使用者
  u8g2.drawStr(0, 10, Sensor_ID.c_str());
  u8g2.drawStr(64, 25, user.c_str());

  // 顯示當前動作 次數
  u8g2.setCursor(0, 40);
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

  // 倒計時秒數和 END 顯示控制
  if (isCountdown) {
    u8g2.setCursor(0, 60);
    u8g2.print("Time: ");
    u8g2.print(remainingTime); // 顯示剩餘時間
  } else {
    if (remainingTime <= 0 && hasStartedCountdown) {
        u8g2.setCursor(0, 60);
        u8g2.print("------------END------------"); // 倒計時結束顯示 END
    }
  }

  u8g2.sendBuffer();
}

// 燒香拜拜不要出錯主程式區
void loop() {
    static unsigned long buttonPressStartTime = 0;
    int buttonState = digitalRead(buttonPin);

    // 按鈕邏輯
    if (buttonState == LOW) {
        if (buttonPressStartTime == 0) {
            buttonPressStartTime = millis();  // 記錄按鈕按下的時間
        }
    } else {
        if (buttonPressStartTime > 0) {
            // 短按邏輯，檢查按鈕按下的時間
            if (millis() - buttonPressStartTime < 1000) {
                if (!isDetecting) {
                    currentMode = (Mode)((currentMode + 1) % 3);  // 切換動作
                    Serial.print("切換動作: ");
                    switch (currentMode) {
                      case PUSH_UP: Serial.println("伏地挺身"); flashOnce(blueColor); break;
                      case SIT_UP: Serial.println("仰臥起坐"); flashOnce(purpleColor); break;
                      case SQUAT: Serial.println("深蹲"); flashOnce(yellowColor); break;
                    }
                    displayText(); // 顯示當前動作
                }
            } else {
                // 長按邏輯
                if (!isDetecting) {
                    isDetecting = true;  // 啟用偵測
                    Serial.println("開始運動偵測");
                    flashOnce(greenColor); // 開始偵測
                } else if (isCountdown && !isCountdownPaused) {
                    isCountdownPaused = true;  // 暫停倒計時
                    Serial.println("暫停倒計時");
                    flashOnce(redColor);
                } else if (isCountdownPaused) {
                    isCountdownPaused = false;  // 恢復倒計時
                    Serial.println("繼續倒計時");
                    flashOnce(greenColor);
                }
            }
            buttonPressStartTime = 0;  // 重置按鈕按下時間
        }
    }

    // 檢測運動狀態
    detectMotion();

    // 處理倒計時邏輯
    if (isCountdown && !isCountdownPaused) {
        unsigned long currentTime = millis();
        if (currentTime - lastSecondUpdate >= 1000) {  // 每秒更新一次
            remainingTime--;  // 減少剩餘時間
            lastSecondUpdate = currentTime;  // 更新上次秒數更新的時間
            displayText();
        }

        // 倒計時結束
        if (remainingTime <= 0) {
            Serial.println("倒計時結束");
            isCountdown = false;
            isDetecting = false; // 停止偵測
            count_push_up = count_sit_up = count_squat = 0; // 歸零次數
            setColor(0, 255, 0); // 綠燈長亮
            displayText(); // 顯示 END 兩秒
            delay(2000); // 延遲2秒顯示 END
            hasStartedCountdown = false;
            displayText(); // 倒數結束后更新顯示
        }
    }
}
