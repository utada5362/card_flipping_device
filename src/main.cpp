#include <Arduino.h>

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

// ========== 蜂鸣器 ==========
void beep(){
  ledcAttachPin(BUZZER, 0);
  ledcSetup(0, 2000, 8);

  ledcWrite(0, 128);   // 开
  delay(1000);
  ledcWrite(0, 0);     // 关
}

// ========== 抽奖 ==========
void lottery(){
  beep();

  int t1 = random(800,2000);
  int t2 = random(800,2000);

  unsigned long start = millis();
  unsigned long T = max(t1,t2);

  while(millis() - start < T){

    if(millis() - start < t1){
      m1.step(true);
      delay(2);
    }

    if(millis() - start < t2){
      m2.step(false);
      delay(2);
    }
  }

  m1.stop();
  m2.stop();
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

  pinMode(BUZZER,OUTPUT);
  digitalWrite(BUZZER,LOW);

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

}