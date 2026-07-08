#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>

#include "image_lottery.h"

// ========== WiFi 配置（请修改为你的WiFi）==========
const char* WIFI_SSID = "Weather";
const char* WIFI_PASS = "2444666668888888";

// ========== NTP 北京时间（UTC+8）==========
const char* NTP_SERVER = "ntp.aliyun.com";
const long GMT_OFFSET = 8 * 3600;       // 北京时间 UTC+8
const int  DST_OFFSET = 0;              // 中国无夏令时

U8G2_SSD1306_128X64_NONAME_F_HW_I2C OLED(U8G2_R0, U8X8_PIN_NONE, 22, 23);

// ========== 电机1 ==========
#define M1_IN1 13
#define M1_IN2 12
#define M1_IN3 14
#define M1_IN4 27

// ========== 电机2 ==========
#define M2_IN1 5
#define M2_IN2 18
#define M2_IN3 19
#define M2_IN4 21

// ========== 按键 ==========
#define SW1 36
#define SW2 39
#define SW3 34

// ========== 蜂鸣器 ==========
#define BUZZER 2

#define STEP_PER_CARD 380

// ========== OLED UI ==========
int uiMode = 0;
unsigned long uiTimer = 0;

// ========== 时间显示 ==========
char timeStr[16];           // "HH:MM:SS" 字符串
bool timeReady = false;     // NTP 是否已同步
unsigned long lastNTP = 0;  // 上次 NTP 更新时间

// ========== 状态 ==========
bool m1_run = false;
bool m2_run = false;

// ========== 双击检测 ==========
unsigned long sw1_last = 0;
unsigned long sw2_last = 0;

int sw1_cnt = 0;
int sw2_cnt = 0;

const int DOUBLE_TIME = 300;

// ========== 电机 ==========
int seq[8][4] = {
  {1,0,0,0},
  {1,1,0,0},
  {0,1,0,0},
  {0,1,1,0},
  {0,0,1,0},
  {0,0,1,1},
  {0,0,0,1},
  {1,0,0,1}
};

class Motor {
public:
  int a,b,c,d;
  int idx = 0;

  Motor(int a_,int b_,int c_,int d_){
    a=a_;b=b_;c=c_;d=d_;
  }

  void begin(){
    pinMode(a,OUTPUT);
    pinMode(b,OUTPUT);
    pinMode(c,OUTPUT);
    pinMode(d,OUTPUT);
    stop();
  }

  void write(int s){
    digitalWrite(a, seq[s][0]);
    digitalWrite(b, seq[s][1]);
    digitalWrite(c, seq[s][2]);
    digitalWrite(d, seq[s][3]);
  }

  void stop(){
    digitalWrite(a,0);
    digitalWrite(b,0);
    digitalWrite(c,0);
    digitalWrite(d,0);
  }

  void step(bool dir){
    if(dir) idx = (idx + 1) & 7;
    else    idx = (idx + 7) & 7;
    write(idx);
  }

  void move(int steps, bool dir, int dly=2){
    for(int i=0;i<steps;i++){
      step(dir);
      delay(dly);
    }
    stop();
  }
};

Motor m1(M1_IN1,M1_IN2,M1_IN3,M1_IN4);
Motor m2(M2_IN1,M2_IN2,M2_IN3,M2_IN4);


// ========== 时间获取 ==========
void updateTime() {
  struct tm info;
  if (getLocalTime(&info)) {
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d",
             info.tm_hour, info.tm_min, info.tm_sec);
    timeReady = true;
  }
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // 等待连接，最多 10 秒
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    configTime(GMT_OFFSET, DST_OFFSET, NTP_SERVER, "pool.ntp.org");

    // 等待首次 NTP 同步
    struct tm info;
    for (int i = 0; i < 20; i++) {
      if (getLocalTime(&info)) {
        timeReady = true;
        break;
      }
      delay(500);
    }
    updateTime();
  }
}

// ========== UI ==========
void showUI(){

  OLED.clearBuffer();

  if(uiMode == 1){
    OLED.setFont(u8g2_font_ncenB14_tr);
    OLED.drawStr(0, 20, "WHITE +1");
  }
  else if(uiMode == 2){
    OLED.setFont(u8g2_font_ncenB14_tr);
    OLED.drawStr(0, 20, "RED +1");
  }
  else if(uiMode == 3){
    OLED.drawXBMP(0,0,128,64,epd_bitmap_20150130224742_AQX8n);
  }
  else{
    OLED.setFont(u8g2_font_6x12_tr);
    OLED.drawStr(0, 12, "Flip Machine");

    // 显示北京时间
    if (timeReady) {
      OLED.setFont(u8g2_font_ncenB18_tr);
      // 居中显示时间
      int w = OLED.getStrWidth(timeStr);
      OLED.drawStr((128 - w) / 2, 50, timeStr);
    } else {
      OLED.setFont(u8g2_font_6x12_tr);
      OLED.drawStr(20, 50, "Time Syncing...");
    }
  }

  OLED.sendBuffer();
}

// ========== UI触发 ==========
void setUI(int mode){
  uiMode = mode;
  uiTimer = millis();
}

// ========== 蜂鸣器 ==========
void beep(int freq = 2000, int duration = 200){
  ledcWriteTone(0, freq);
  delay(duration);
  ledcWriteTone(0, 0);
}

// ========== 抽奖 ==========
void lottery(){

  
  beep();
  setUI(3);

  // ===== 随机目标牌数 =====
  int card1 = random(10, 25);
  int card2 = random(10, 25);

  int steps1 = card1 * STEP_PER_CARD;
  int steps2 = card2 * STEP_PER_CARD;

  int totalSteps = max(steps1, steps2);

  unsigned long lastUI = 0;

  for(int i = 0; i < totalSteps; i++){

    // ===== 计算减速系数 =====
    float progress = (float)i / totalSteps;

    int delayTime;

    // ===== 三段减速逻辑 =====
    if(progress < 0.5){
      delayTime = 2;
    }
    else if(progress < 0.8){
      delayTime = 2 + (int)((progress - 0.5) * 20);
    }
    else{
      delayTime = 10 + (int)((progress - 0.8) * 80);
    }

    // ===== 电机1 =====
    if(i < steps1){
      m1.step(true);
    }

    // ===== 电机2（反向）=====
    if(i < steps2){
      m2.step(false);
    }

    // ===== 更新OLED显示（每50ms一次，避免I2C阻塞电机）=====
    if(millis() - lastUI > 50){
      showUI();
      lastUI = millis();
    }

    delay(delayTime);
  }

  m1.stop();
  m2.stop();

  // 抽奖结束，切回默认文字
  setUI(0);
}

// ================= SW1 =================
void handleSW1(){

  if(digitalRead(SW1) == LOW){
    delay(20);
    if(digitalRead(SW1) == LOW){
      while(digitalRead(SW1) == LOW);

      unsigned long now = millis();

      if(now - sw1_last < DOUBLE_TIME){
        m1_run = !m1_run;   // ✔ 双击：切换运行/暂停
        sw1_cnt = 0;
      } 
      else {
        sw1_cnt = 1;        // ✔ 单击标记
      }

      sw1_last = now;
    }
  }

  // ✔ 单击超时 → 翻牌
  if(sw1_cnt == 1 && millis() - sw1_last > DOUBLE_TIME){
    m1.move(STEP_PER_CARD,true);
    setUI(1);
    sw1_cnt = 0;
  }
}

// ================= SW2 =================
void handleSW2(){

  if(digitalRead(SW2) == LOW){
    delay(20);
    if(digitalRead(SW2) == LOW){
      while(digitalRead(SW2) == LOW);

      unsigned long now = millis();

      if(now - sw2_last < DOUBLE_TIME){
        m2_run = !m2_run;
        sw2_cnt = 0;
      } 
      else {
        sw2_cnt = 1;
      }

      sw2_last = now;
    }
  }

  if(sw2_cnt == 1 && millis() - sw2_last > DOUBLE_TIME){
    m2.move(STEP_PER_CARD,false);
    setUI(2);
    sw2_cnt = 0;
  }
}

// ========== setup ==========
void setup(){
  Serial.begin(115200);

  m1.begin();
  m2.begin();

  pinMode(SW1,INPUT);
  pinMode(SW2,INPUT);
  pinMode(SW3,INPUT);

  ledcSetup(0, 2000, 8);
  ledcAttachPin(BUZZER, 0);
  ledcWrite(0, 0);

  // 启动诊断蜂鸣
  beep(2000, 200);

  OLED.begin();
  OLED.clearBuffer();
  OLED.sendBuffer();

  initWiFi();

  randomSeed(analogRead(0));
}

// ========== loop ==========
void loop(){

  handleSW1();
  handleSW2();

  if(digitalRead(SW3) == LOW){
    delay(20);
    if(digitalRead(SW3) == LOW){
      while(digitalRead(SW3) == LOW);
      lottery();
    }
  }

  if(m1_run){
    m1.step(true);
    delay(2);
  }

  if(m2_run){
    m2.step(false);
    delay(2);
  }
  if(millis() - uiTimer > 3000){
    uiMode = 0;
  }

  // 每秒更新一次北京时间
  if (millis() - lastNTP > 1000) {
    updateTime();
    lastNTP = millis();
  }

  static unsigned long lastOLED = 0;
  if(millis() - lastOLED > 50){
    showUI();
    lastOLED = millis();
  }
}


 /*
 #include <Arduino.h>

#define BUZZER 2

void setup() {
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
}

void loop() {

  // 响
  digitalWrite(BUZZER, HIGH);
  delay(1000);

  // 停
  digitalWrite(BUZZER, LOW);
  delay(1000);
}
  */