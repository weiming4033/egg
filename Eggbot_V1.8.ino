#include <Event.h>
#include <Timer.h>
#include <Wire.h>
#include <PlainProtocol.h>
#include <SoftwareSerial.h>
#include <SerialDebug.h>
#include <FastLED.h>
#include <Motor.h>
#include <SPI.h>
#include <RFID.h>
#include "LED.h"
#include <avr/wdt.h>
#include <EEPROM.h>
#include <QueueList.h> //列表
//AT+SETTING=DEFAULT
#define PT_USE_TIMER
#define PT_USE_SEM
#include "pt.h"
static struct pt thread1,thread2,thread3,thread5,thread6; //创建三个任务
static struct pt_sem sem_NFC_B;
static struct pt_sem sem_NFC_F;
static struct pt_sem sem_Dog;
static struct pt_sem sem_Random;
static struct pt_sem sem_Rotate;

#define MANUAL_MODE 1
#define LINE_FOLLOW_MODE 2
#define PROGRAMMER_MODE 3
#define NFC_MODE 4
#define DOG_MODE 5
#define RANDOM_MODE 7

#define SERIAL_DEBUG_SEPARATOR ","

PlainProtocol BotCom(Serial, 115200);
SoftwareSerial soundSerial(3, 2);

// create a queue of strings messages.
QueueList <uint8_t> queue;

CRGB leds[NUM_LEDS];
Timer nfc_t;

Motor eggMotor;
RFID bottom_rfid(10);    //D10--片选引脚
RFID front_rfid(8);    //D8--片选引脚

int headLEDR = 0;
int headLEDG = 0;
int headLEDB = 0;
int earLEDR = 0;
int earLEDG = 0;
int earLEDB = 0;
int baseLEDR = 0;
int baseLEDG = 0;
int baseLEDB = 0;

uint8_t bottom_success = 0;
uint8_t data[16] = {0,0,0,};
uint8_t front_success = 0;
uint8_t front_data[16] = {0,0,0,};
int watchdog = 0;
int nfcStopRfid = 0;

bool flag = true;  // for two nfc work 100ms
bool magicFlag = true; //for rgb show
bool flagGo = true; //for nfc line

bool flagGoRandom = false; //for random
uint8_t random_step = 0; //for random
uint8_t random_direction = 0; //for random
uint8_t move = 0; 
bool turnLeftFlag = false;
bool turnRightFlag = false;
bool deadFlag = false;
bool endFlag = true;
bool manualLine = false;

//int mode = MANUAL_MODE;
int mode = LINE_FOLLOW_MODE;
//int mode = NFC_MODE;

int countSound = 0; //萌宠时间增加
int eggStat = 0; //萌宠当前状态
int repeatCount = 0; //播放声音重复次数

int addr = 0; //eeprom adress
byte ee_id = 0; //读取eeprom的id值

void setup()
{
  //EEPROM.write(addr, 1); //白色
  //EEPROM.write(addr, 2); //蓝色
  //EEPROM.write(addr, 3); //黄色
  //白色 -- 01 蓝色 -- 02 黄色 -- 03
  ee_id = EEPROM.read(addr);
  BotCom.init();
  eggMotor.initMotor();
  eggMotor.runningMode = SPEED;
  initRGB();
  //initLineFollow(float kp, float ki, float kd, int threshold, int power)
  //eggMotor.initLineFollow(4.6, 2, 1, 650, 70);  //白
  eggMotor.initLineFollow(4.6, 2, 1, 650, 60);  //蓝
  //eggMotor.initLineFollow(4.6, 2, 1, 650, 90);  //黄

  //eggMotor.initLineFollow(1.3, 2.4, 1, 650, 100);  //白
  //eggMotor.initLineFollow(1, 5, 1, 650, 90);  //黄
  soundSerial.begin(9600);
  bottom_rfid.rc522Init();
  delay(500);
  front_rfid.rc522Init();
  delay(1000);

  int tickEvent = nfc_t.every(100, setNFC);

  PT_SEM_INIT(&sem_NFC_B,0); 
  PT_SEM_INIT(&sem_NFC_F,0); 
  PT_SEM_INIT(&sem_Dog,0); 
  PT_SEM_INIT(&sem_Random,0); 
  PT_SEM_INIT(&sem_Rotate,0); 

  //初始化任务记录变量
  PT_INIT(&thread1);
  PT_INIT(&thread2);
  PT_INIT(&thread3);
  PT_INIT(&thread5);
  PT_INIT(&thread6);

  showOffAll(); //all led off
  
  volSound(5);
  soundSerial.flush();
  delay(1000);
  
  soundSerial.flush();
  soundSerial.print("play,0010,$");
  soundSerial.flush();

  eggMotor.timerId = 0;
  eggMotor.inMotion = 0;
  randomSeed(analogRead(A0));

  wdt_enable(WDTO_8S);    // enable the watchdog timer : 1 second timer
}

int thread1_entry(struct pt *pt)
{
    PT_BEGIN(pt);
    while (1)
    {
        PT_SEM_WAIT(pt,&sem_NFC_B); 
        bottom_success = bottom_rfid.readNFC522(data);
        //PT_TIMER_DELAY(pt,1000);//留一秒
        PT_YIELD(pt); 
    }
    PT_END(pt);
}

int thread2_entry(struct pt *pt)
{
    PT_BEGIN(pt);
    while (1)
    {
        PT_SEM_WAIT(pt,&sem_NFC_F);
        front_success = front_rfid.readNFC522(front_data);       
        PT_YIELD(pt); 
    }
    PT_END(pt);
}

int thread3_entry(struct pt *pt)
{
    PT_BEGIN(pt);
    while (1)
    {
        PT_SEM_WAIT(pt,&sem_Dog);
        wdt_reset();    // feed the dog
        if(eggStat == 1 && repeatCount < 3){
          repeatCount++;
          showLeft(70+random(10, 80), 40+random(10, 80), 0);
          showRight(70+random(10, 80), 40+random(10, 80), 0); 
          //PT_TIMER_DELAY(pt,3000);//留一秒
          playSound(95); //提出吃饭
          PT_TIMER_DELAY(pt,5000);//留一秒
        }
        else if(eggStat == 2 && repeatCount < 3){
            repeatCount++;
            showLeft(70+random(10, 80), 40+random(10, 80), 0);
            showRight(70+random(10, 80), 40+random(10, 80), 0); 
            //PT_TIMER_DELAY(pt,3000);//留一秒
            playSound(96); //提出吃水果
            PT_TIMER_DELAY(pt,5000);//留一秒
        }
        else if(eggStat == 3 && repeatCount < 3){
            repeatCount++;
            showLeft(70+random(10, 80), 40+random(10, 80), 0);
            showRight(70+random(10, 80), 40+random(10, 80), 0); 
            //PT_TIMER_DELAY(pt,3000);//留一秒
            playSound(97); //提出刷牙
            PT_TIMER_DELAY(pt,5000);//留一秒
        }
        else if(eggStat == 4 && repeatCount < 3){
            repeatCount++;
            showLeft(70+random(10, 80), 40+random(10, 80), 0);
            showRight(70+random(10, 80), 40+random(10, 80), 0); 
            //PT_TIMER_DELAY(pt,3000);//留一秒
            playSound(98); //提出上厕所
            PT_TIMER_DELAY(pt,5000);//留一秒
        }
        else if(eggStat == 5 && repeatCount < 3){
            repeatCount++;
            showLeft(70+random(10, 80), 40+random(10, 80), 0);
            showRight(70+random(10, 80), 40+random(10, 80), 0); 
            //PT_TIMER_DELAY(pt,3000);//留一秒
            playSound(99); //提出唱歌
            PT_TIMER_DELAY(pt,5000);//留一秒
        }
        else if(eggStat == 6 && repeatCount < 3){
            repeatCount++;
            showLeft(70+random(10, 80), 40+random(10, 80), 0);
            showRight(70+random(10, 80), 40+random(10, 80), 0); 
            //PT_TIMER_DELAY(pt,3000);//留一秒
            playSound(100); //提出运动
            PT_TIMER_DELAY(pt,5000);//留一秒
        }
        else if(eggStat == 7 && repeatCount < 3){
            repeatCount++;
            showLeft(70+random(10, 80), 40+random(10, 80), 0);
            showRight(70+random(10, 80), 40+random(10, 80), 0); 
            //PT_TIMER_DELAY(pt,3000);//留一秒
            playSound(101); //提出喝水
            PT_TIMER_DELAY(pt,5000);//留一秒
        }
        else if(eggStat == 8 && repeatCount < 3){
            repeatCount++;
            showLeft(70+random(10, 80), 40+random(10, 80), 0);
            showRight(70+random(10, 80), 40+random(10, 80), 0); 
            //PT_TIMER_DELAY(pt,3000);//留一秒
            playSound(102); //提出洗澡
            PT_TIMER_DELAY(pt,5000);//留一秒
        }
        else if(eggStat == 9 && repeatCount < 3){
            repeatCount++;
            showLeft(70+random(10, 80), 40+random(10, 80), 0);
            showRight(70+random(10, 80), 40+random(10, 80), 0); 
            //PT_TIMER_DELAY(pt,3000);//留一秒
            playSound(103); //提出治疗
            PT_TIMER_DELAY(pt,5000);//留一秒
        }   
        PT_YIELD(pt); 
    }
    PT_END(pt);
}

int thread5_entry(struct pt *pt)
{
    PT_BEGIN(pt);
    while (1)
    {
        PT_SEM_WAIT(pt,&sem_Random); 
        wdt_reset();    // feed the dog   
        //1代表开始后向前走 
        if(random_direction == 1){
            queue.push(random_direction);
            random_direction = 0;
        }
        //2代表先向左旋转90度，再向前走一步
        else if(random_direction == 2){
            queue.push(random_direction);
            random_direction = 0;
        }
        else if(random_direction == 3){
            queue.push(random_direction);
            random_direction = 0;
        }
        PT_YIELD(pt); 
    }
    PT_END(pt);
}

int thread6_entry(struct pt *pt)
{
    PT_BEGIN(pt);
    while (1)
    {
        PT_SEM_WAIT(pt,&sem_Rotate); 
        wdt_reset();    // feed the dog  
        if(random_step == 2 && flagGoRandom == false){
            //刷确定 从队列中弹出 依次执行
            if(queue.isEmpty() == false){ //队列中有数据
                //如果队列没有空
                move = queue.pop();
                if(move == 1){
                    //代表向前走
                    flagGoRandom = true;
                    PT_TIMER_DELAY(pt,500);//留一秒
                    deadFlag = true;
                    endFlag = true;
                }
                else if(move == 2){
                  //代表向左走
                    turnLeftFlag = true;
                    PT_TIMER_DELAY(pt,2000);//留一秒
                    flagGoRandom = true;
                    PT_TIMER_DELAY(pt,500);//留一秒
                    deadFlag = true;
                    endFlag = true;
                }
                else if(move == 3){
                    //代表向右走
                    turnRightFlag = true;
                    PT_TIMER_DELAY(pt,2000);//留一秒
                    flagGoRandom = true;
                    PT_TIMER_DELAY(pt,500);//留一秒
                    deadFlag = true;
                    endFlag = true;
                }
            }
            else{
                move = 0; //如果队列为空, 刚置0
                flagGoRandom = false;
                random_step = 1; //复位
            }
            
        }
        else if(random_step == 3){
            //刷取消，清空queue, 
            for(int i=0; i< queue.count(); i++){
                queue.pop();
            }
            random_step = 1; //取消后，清空队列后，重新开始
        }
        PT_YIELD(pt); 
    }
    PT_END(pt);
}

void setNFC()
{
    if(mode == MANUAL_MODE)
    {
      watchdog++;
    }
    if(flag){
        PT_SEM_SIGNAL(pt,&sem_NFC_F);
    } else {
        PT_SEM_SIGNAL(pt,&sem_NFC_B);     
    }
    flag = !flag;
    //开启宠物功能
    if(mode == DOG_MODE){
        //默认没有开启宠物功能，直接返回
        countSound++;
        if(countSound >= 3000){
            countSound = 0;
        }
        //10  -- 1s 100 -- 10s 
        switch (countSound) {
            case 50://吃饭
                eggStat = 1;
                repeatCount = 0;
                break;
            case 350://水果
                eggStat = 2;
                repeatCount = 0;
                break;
            case 650://刷牙
                eggStat = 3;
                repeatCount = 0;
                break;
            case 950://拉屎
                eggStat = 4;
                repeatCount = 0;
                break;
            case 1250://放音乐
                eggStat = 5;
                repeatCount = 0;
                break;
            case 1550://运动
                eggStat = 6;
                repeatCount = 0;
                break;
            case 1850://喝水
                eggStat = 7;
                repeatCount = 0;
                break;
            case 2150://洗澡
                eggStat = 8;
                repeatCount = 0;
                break;
            case 2450://生病了
                eggStat = 9;
                repeatCount = 0;
                break;
            default:
              break;
        }
    }
}

void program2bot()
{
  //speedCtrl(vbot, wbot);
}

void playSound(int sound) {
  int soundId = 0x00 + sound;  //0x20
  String cmd;
  if(soundId <= 9){
      cmd = "play,000";
  }
  else if(soundId <= 99){
    cmd = "play,00";
  }
  else if(soundId <= 999){
    cmd = "play,0";
  }
  cmd += String(soundId, DEC);
  cmd += ",$";
  soundSerial.flush();
  soundSerial.print(cmd);
  soundSerial.flush();
}

void volSound(int loud) {
  String cmd;
  String upHex;
  cmd = "vol,";
  upHex = String(loud, HEX);
  upHex.toUpperCase();
  cmd += upHex;
  cmd += ",$";
  soundSerial.flush();
  soundSerial.print(cmd);
  soundSerial.flush();
}

void loop()
{
  wdt_reset();    // feed the dog
  cmdProcess();

  if(watchdog >= 2){
    eggMotor.stopRobot();
    watchdog = 0;
    eggMotor.v = 0;
    eggMotor.w = 0;
    return;
  }

  switch (mode)
  {
      case MANUAL_MODE:
        if(eggMotor.runningMode == SPEED){
              eggMotor.moveBase();
        } 
        break;
      case LINE_FOLLOW_MODE:
        if(eggMotor.nfc_in == 3 || eggMotor.nfc_in == 2){
            if(flagGo){
                eggMotor.followLine();
            }
            //eggMotor.followLine();
        }
        break;
      case PROGRAMMER_MODE:           
          //program2bot();
          break;
      case NFC_MODE:
            eggMotor.inMotion = 0;
            break;
      case DOG_MODE://吃饭 水果 刷牙 拉屎 放音乐 运动 喝水 洗澡 生病了 
            //dogProcess();
            PT_SEM_SIGNAL(pt,&sem_Dog);
            break;
      case RANDOM_MODE:
            PT_SEM_SIGNAL(pt,&sem_Random);
            PT_SEM_SIGNAL(pt,&sem_Rotate);
            if(turnLeftFlag){
                eggMotor.turnToLine(1); //向左转到巡线
                if(eggMotor.stepLine == 2){
                    eggMotor.stepLine = 0;
                    turnLeftFlag = false;
                }
            }
            if(turnRightFlag){
                eggMotor.turnToLine(2); //向右转到巡线
                if(eggMotor.stepLine == 2){
                    eggMotor.stepLine = 0;
                    turnRightFlag = false;
                }
            }
            if(eggMotor.nfc_in == 5){ //5为随机寻找水果模式
                if(flagGoRandom){ //true为向前走
                    manualLine = (eggMotor.isStop && endFlag);
                    //eggMotor.followLine();
                    if(manualLine){
                        eggMotor.setWheelSpeed(50,50); //遇到边角直走1s
                        delay(500);
                        eggMotor.setWheelSpeed(0,0); //停车
                        endFlag = false;
                    }
                    else{
                        eggMotor.followLine();
                    }
                }
            }
      default:  
          break;
  }

    thread1_entry(&thread1); //底部nfc读取
    thread2_entry(&thread2); //前面nfc读取
    thread3_entry(&thread3); //萌宠播放模式
    thread5_entry(&thread5); //随机播放模式
    thread6_entry(&thread6); //随机播放模式

    bottomProcess();
    frontProcess();

    eggMotor.t.update();
    nfc_t.update();
}

void frontProcess()
{
  if(front_success)
  {
      //mode = NFC_MODE;
      eggMotor.runningMode = NFCMOVE;
      switch (front_data[0]) 
      {
        case 0x01:
    //      Serial.println(front!");
            playSound(front_data[2]);
            eggMotor.motion(front_data[1]);  //01 01 01
            eggMotor.nfc_in = 1;
            break;
        case 0x02:
    //        Serial.println("introduce");
            if(ee_id == front_data[1]){ //当蛋玩id与卡边相符时，才进行相应的动作
                playSound(front_data[2]);  //02 20
                eggMotor.nfc_in = 1;
                showLeft(70+random(10, 80), 40+random(10, 80), 0);
                showRight(70+random(10, 80), 40+random(10, 80), 0); 
            }
            break;
        case 0x03:
    //      Serial.println("back");
            playSound(front_data[2]);
            eggMotor.motion(front_data[1]);  // 03 02 02
            eggMotor.nfc_in = 1;
            break;
        case 0x04:
    //      Serial.println("left");
            playSound(front_data[2]);
            eggMotor.motion(front_data[1]);  // 04 03 03
            eggMotor.nfc_in = 1;
            break;
        case 0x05:
    //      Serial.println("right");
            playSound(front_data[2]);
            eggMotor.motion(front_data[1]);  // 05 04 04
            eggMotor.nfc_in = 1;
            break;
          case 0x10:
    //        Serial.println("horse");
            playSound(front_data[2]); // 10 00 34  
            showLeft(70+random(10, 80), 40+random(10, 80), 0);
            showRight(70+random(10, 80), 40+random(10, 80), 0);
            break;
          case 0x11: //机器人
    //        Serial.println("robot");
            playSound(front_data[2]); // 11 00 35  
            eggMotor.nfc_in = 1;
            eggMotor.motion(random(3, 5));
            delay(1500);
            eggMotor.motion(random(3, 5));
            showLeft(70+random(10, 80), 40+random(10, 80), 0);
            showRight(70+random(10, 80), 40+random(10, 80), 0);      
            break;
          case 0x12:
    //        Serial.println("pencil");
            playSound(front_data[2]); // 12 00 36  
            showLeft(70+random(10, 80), 40+random(10, 80), 0);
            showRight(70+random(10, 80), 40+random(10, 80), 0); 
            break;
          case 0x13:
    //        Serial.println("ballon");
            showLeft(70+random(10, 80), 40+random(10, 80), 0);
            showRight(70+random(10, 80), 40+random(10, 80), 0);
            playSound(front_data[2]); // 13 00 37 
            break;  
          case 0x14:
    //        Serial.println("english");
            playSound(front_data[2]); // 14 00 38   
            break;
          case 0x15:
    //        Serial.println("intro");
            playSound(front_data[2]); // 15 00 39   
            break;

          case 0x60: //随机找卡模式
            mode = RANDOM_MODE; //进入随机寻找模式
            eggMotor.nfc_in = 5; //5代表随机巡线模式
            random_step = 1; //1代表第一步开始
            playSound(202); //开始旅程吗
            delay(1000);
            break; 

          case 0x61: //前
            if(random_step == 1){
                playSound(210); //向前走音效
                delay(500);
                random_direction = 1; //1代表开始后向前走 
            }
            break; 

          case 0x62: //左
            if(random_step == 1){
                playSound(211); //向左走音效
                delay(500);
                random_direction = 2;//2代表先向左旋转90度，再向前走一步
            }
            break; 

          case 0x63: //右
            if(random_step == 1){
                playSound(212); //向右走音效
                delay(500);
                random_direction = 3;//3代表先向右旋转90度，再向前走一步
            }
            break; 

          case 0x64: //确认
                playSound(13); //向前走音效
                delay(2000);
                random_step = 2;//2代表确认完成
            break; 

          case 0x65: //取消
            if(random_step == 1){
                playSound(12); //取消音效
                delay(2000);
                random_step = 3;//3代表取消编程
            }
            break;

        case 0x68: //开启萌宠模式
            mode = DOG_MODE;
            repeatCount = 0;
            eggStat = 0;
            playSound(1); //学狗叫
            showLeft(70+random(10, 80), 40+random(10, 80), 0);
            showRight(70+random(10, 80), 40+random(10, 80), 0); 
            break;

        case 0x67: //关闭萌宠模式
            mode = LINE_FOLLOW_MODE;
            repeatCount = 0;
            eggStat = 0;
            countSound = 0;
            playSound(2); //学大象叫
            showLeft(70+random(10, 80), 40+random(10, 80), 0);
            showRight(70+random(10, 80), 40+random(10, 80), 0); 
            break;

        case 0x69:
            switch (front_data[1]) 
            {//吃饭 水果 刷牙 拉屎 放音乐 运动 喝水 洗澡 生病了 
                case 1:
                    if(eggStat == 1){
                        eggStat = 0;
                        playSound(104);//吃饭
                        showLeft(70+random(10, 80), 40+random(10, 80), 0);
                        showRight(70+random(10, 80), 40+random(10, 80), 0); 
                    }
                  break;
                case 2:
                    if(eggStat == 2){
                        eggStat = 0;
                        playSound(105);//水果
                        showLeft(70+random(10, 80), 40+random(10, 80), 0);
                        showRight(70+random(10, 80), 40+random(10, 80), 0); 
                    }
                  break;
                case 3:
                    if(eggStat == 3){
                        eggStat = 0;
                        playSound(106);//刷牙
                        showLeft(70+random(10, 80), 40+random(10, 80), 0);
                        showRight(70+random(10, 80), 40+random(10, 80), 0); 
                    }
                  break;
                case 4:
                    if(eggStat == 4){
                        eggStat = 0;
                        playSound(107);//shit
                        showLeft(70+random(10, 80), 40+random(10, 80), 0);
                        showRight(70+random(10, 80), 40+random(10, 80), 0); 
                    }
                  break;
                case 5:
                    if(eggStat == 5){
                        eggStat = 0;
                        playSound(random(115, 126)); // 11 00 35 放音乐
                        showLeft(70+random(10, 80), 40+random(10, 80), 0);
                        showRight(70+random(10, 80), 40+random(10, 80), 0); 
                    }
                  break;
                case 6:
                    if(eggStat == 6){
                        eggStat = 0;
                        playSound(108); //运动
                        showLeft(70+random(10, 80), 40+random(10, 80), 0);
                        showRight(70+random(10, 80), 40+random(10, 80), 0); 
                    }
                  break;
                case 7:
                    if(eggStat == 7){
                        eggStat = 0;
                        playSound(109); //喝水
                        showLeft(70+random(10, 80), 40+random(10, 80), 0);
                        showRight(70+random(10, 80), 40+random(10, 80), 0); 
                    }
                  break;
                case 8:
                    if(eggStat == 8){
                        eggStat = 0;
                        playSound(110); //洗澡
                        showLeft(70+random(10, 80), 40+random(10, 80), 0);
                        showRight(70+random(10, 80), 40+random(10, 80), 0); 
                    }
                  break;
                case 9:
                    if(eggStat == 9){
                        eggStat = 0;
                        playSound(111); //生病了
                        showLeft(70+random(10, 80), 40+random(10, 80), 0);
                        showRight(70+random(10, 80), 40+random(10, 80), 0); 
                    }
                  break;
                default:
                    break;
            }
            break;
        case 0x66: //随机播放音乐
            playSound(random(115, 126)); 
            eggMotor.nfc_in = 1;
            showLeft(70+random(10, 80), 40+random(10, 80), 0);
            showRight(70+random(10, 80), 40+random(10, 80), 0); 
            break;

        case 0x77:
          eggMotor.nfc_in = 2; //进入nfc停车模式 ，停车后播放相应的音乐
          flagGo = true;  //向前走 
          nfcStopRfid = front_data[1];
          if(magicFlag){
              showLeft(170+random(10, 80), 140+random(10, 80), 0);
              showRight(170+random(10, 80), 140+random(10, 80), 0);   
          }else{
              showLeft(80+random(10, 80), 150+random(10, 80), 0);
              showRight(80+random(10, 80), 150+random(10, 80), 0);
          }
          magicFlag = !magicFlag;
          break;

        case 0x88:
          eggMotor.nfc_in = 3; //进入巡线模式 底部nfc遇到卡就停车 ，并播放相应的音乐
          flagGo = true;  //向前走 
          if(magicFlag){
              showLeft(170+random(10, 80), 140+random(10, 80), 0);
              showRight(170+random(10, 80), 140+random(10, 80), 0);   
          }else{
              showLeft(80+random(10, 80), 150+random(10, 80), 0);
              showRight(80+random(10, 80), 150+random(10, 80), 0);
          }
          magicFlag = !magicFlag;
          break;

        case 0x99: //校正巡线模式的值
            playSound(5);
            flagGo = false;
            showLeft(100+random(10, 20), 200, 0);
            showRight(100+random(10, 20), 200, 0);
            eggMotor.calibrate();
            delay(1000);
            showOffAll();
            break;
        default:
          break;
      }
    front_success = !front_success;
  }
}

void bottomProcess()
{
  if (bottom_success)
  {
    //mode = NFC_MODE;
    bool bottomFlag = false;
    eggMotor.stopRfid = data[3];
    eggMotor.runningMode = NFCMOVE;
    bottomFlag = (data[0] >= 58 && data[0] <= 94 && eggMotor.nfc_in == 3);
    if(bottomFlag){
        playSound(data[0]);
        eggMotor.stopRobot();
        eggMotor.v = 0;
        eggMotor.w = 0;
        flagGo = false;
    }
    bottomFlag = (data[3] >= 40 && data[3] <= 57 && eggMotor.nfc_in == 3);
    if(bottomFlag){
        playSound(data[3]);
        eggMotor.stopRobot();
        eggMotor.v = 0;
        eggMotor.w = 0;
        flagGo = false;
    }

    bottomFlag = (data[3] == nfcStopRfid && eggMotor.nfc_in == 2);
    if(bottomFlag){  
        playSound(data[3]);
        eggMotor.stopRobot();
        eggMotor.v = 0;
        eggMotor.w = 0;
        flagGo = false;
    }

    bottomFlag = (((data[4] >= 1 && data[4] <= 4) || (data[5] >= 1 && data[5] <= 6)) && eggMotor.nfc_in == 5) && deadFlag == true;
    if(bottomFlag){
        playSound(data[4]+205);
        delay(100);
        eggMotor.stopRobot();
        eggMotor.v = 0;
        eggMotor.w = 0;
        flagGoRandom = false;
        deadFlag = false; //死区时间500ms
    }

    bottom_success = !bottom_success;
  } //endif
}

void cmdProcess()
{
  int soundnum = 0;
  int distance = 0;
  int angle = 0;
  int linearvelocity = 0;
  int angularvelocity = 0;
  int delaytime = 0;
  int r = 0;
  int g = 0;
  int b = 0;
  int light_position = 0;
  int modestatus = 0;
  if (BotCom.available()) {
    if (BotCom.equals("velocity")) {
      eggMotor.v = BotCom.read();
      eggMotor.w = BotCom.read();
      watchdog = 0;
      //DEBUG("raw velocity", v, w);
      if(eggMotor.v != 0 || eggMotor.w !=0){
          //mode = MANUAL_MODE;
          eggMotor.runningMode = SPEED;
      }
      eggMotor.v = constrain(eggMotor.v, -200, 200);
      eggMotor.w = constrain(eggMotor.w, -200, 200);
      eggMotor.v = map(eggMotor.v, -200, 200, -900, 900);
      eggMotor.w = map(eggMotor.w, -200, 200, -6, 6);

    } else if (BotCom.equals("start")) {
      //DEBUG("In Programming Mode");
      mode = PROGRAMMER_MODE;
    } else if (BotCom.equals("stop")) {
      //DEBUG("STOP Programming Mode");
      //mode = MANUAL_MODE;
      eggMotor.stopRobot();  //comment
    }  else if (BotCom.equals("mode")) {
      modestatus = BotCom.read();
      if (modestatus == 0) {
        mode = MANUAL_MODE; 
        if(eggMotor.nfc_in == 1){ //1 为motion模式
            
        }else if(eggMotor.nfc_in == 2){ //2为nfc停车模式
          eggMotor.stopRobot();
          //eggMotor.nfc_in = 0;
        }
        else if(eggMotor.nfc_in == 3){ //3为直接巡线模式
          eggMotor.stopRobot();
          //eggMotor.nfc_in = 0;
        }
      } else{
        //eggMotor.runningMode = NFCMOVE;
        mode = LINE_FOLLOW_MODE;
        //eggMotor.nfc_in = 3;
        if(eggMotor.nfc_in == 2){
             //eggMotor.nfc_in = 2;  //3为直接巡线模式
        }else if(eggMotor.nfc_in == 0){
           //eggMotor.nfc_in = 3;
         }
      }
    }  else if (BotCom.equals("move_f")) {
      linearvelocity = BotCom.read();
      //DEBUG("move front", linearvelocity);
      linearvelocity = constrain(linearvelocity, -200, 200);
      linearvelocity = map(linearvelocity, -200, 200, -1142, 1142);
      //DEBUG("move front real", linearvelocity);
      eggMotor.speedCtrl(linearvelocity, 0);
    } else if (BotCom.equals("move_b")) {
      linearvelocity = BotCom.read();
     // DEBUG("move back", linearvelocity);
      linearvelocity = constrain(-linearvelocity, -200, 200);
      linearvelocity = map(linearvelocity, -200, 200, -1142, 1142);
      eggMotor.speedCtrl(linearvelocity, 0);
    } else if (BotCom.equals("move_tf")) {
      linearvelocity = BotCom.read();
      distance = BotCom.read();
      //DEBUG("raw move forward to", linearvelocity, distance);
      linearvelocity = constrain(linearvelocity, -200, 200);
      linearvelocity = map(linearvelocity, -200, 200, -1142, 1142);
      //DEBUG("move forward to", linearvelocity, distance);
      eggMotor.moveForward(distance, linearvelocity);
    } else if (BotCom.equals("move_tb")) {
      linearvelocity = BotCom.read();
      distance = BotCom.read();
      //DEBUG("move backward to", linearvelocity, distance);
      linearvelocity = constrain(linearvelocity, -200, 200);
      linearvelocity = map(linearvelocity, -200, 200, -1142, 1142);
      eggMotor.moveBackward(distance, linearvelocity);
    } else if (BotCom.equals("turn_l")) {
      angularvelocity = BotCom.read();
      //DEBUG("turn left", angularvelocity);
      eggMotor.speedCtrl(0, angularvelocity);
    } else if (BotCom.equals("turn_r")) {
      angularvelocity = BotCom.read();
      //DEBUG("turn right", angularvelocity);
      eggMotor.speedCtrl(0, -angularvelocity);
    } else if (BotCom.equals("turn_tl")) {
      angularvelocity = BotCom.read();
      angle = BotCom.read();
      //DEBUG("turn left to", angularvelocity, angle);
      eggMotor.turnLeft(angle, angularvelocity);
    } else if (BotCom.equals("turn_tr")) {
      angularvelocity = BotCom.read();
      angle = BotCom.read();
      //DEBUG("turn right to", angularvelocity, angle);
      eggMotor.turnRight(angle, angularvelocity);
    } else if (BotCom.equals("light")) {
      light_position = BotCom.read();
      r = BotCom.read();
      g = BotCom.read();
      b = BotCom.read();
      //DEBUG("light show", light_position, r, g, b);
      //showLeft(r, g, b);
      //FastLED.show();
    } else if (BotCom.equals("wait")) {
      delaytime = BotCom.read();
      delay(delaytime);
      //DEBUG("wait", delaytime);
      eggMotor.stopRobot();  //comment
    } else if (BotCom.equals("end")) {
      //DEBUG("END LOOP");
      mode = MANUAL_MODE;
      eggMotor.stopRobot();  //comment
    } else if (BotCom.equals("headrgb")) {
        headLEDR = BotCom.read();
        headLEDG = BotCom.read();
        headLEDB = BotCom.read();
        showHead(headLEDR, headLEDG, headLEDB);
    } else if (BotCom.equals("earrgb")) {
        earLEDR = BotCom.read();
        earLEDG = BotCom.read();
        earLEDB = BotCom.read();
        showLeft(earLEDR, earLEDG, earLEDB);
      //DEBUG("EAR RGB", earLEDR, earLEDG, earLEDB);
    } else if (BotCom.equals("basergb")) {
        baseLEDR = BotCom.read();
        baseLEDG = BotCom.read();
        baseLEDB = BotCom.read();
        showRight(baseLEDR, baseLEDG, baseLEDG);
        //FastLED.show();
    } else if (BotCom.equals("sound")) {
      eggMotor.stopRobot();  //comment
      soundSerial.flush();
      delay(5);
      soundnum = BotCom.read();
      //DEBUG("SOUND", soundnum);
      switch (soundnum) {
        case 1:
          soundSerial.print("play,0001,$");
          break;
        case 2:
          soundSerial.print("play,0002,$");
          break;
        case 3:
          soundSerial.print("play,0003,$");
          break;
        case 4:
          soundSerial.print("play,0004,$");
          break;
        case 5:
          soundSerial.print("play,0005,$");
          break;
        case 6:
          soundSerial.print("play,0006,$");
          break;
        case 7:
          soundSerial.print("play,0007,$");
          break;
        case 8:
          soundSerial.print("play,0008,$");
          //mode = LINE_FOLLOW_MODE;
          break;
        case 9:
          soundSerial.print("play,0009,$");
          //mode = MANUAL_MODE;
          break;
        default:
          break;
      }
    }
  }
}
