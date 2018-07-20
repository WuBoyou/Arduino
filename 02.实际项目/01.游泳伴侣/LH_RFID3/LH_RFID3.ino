// 2017.06.03
// Copyright Wu BoYou

// 【类库定义】
// OLED显示屏
#include "U8glib.h"
// 伺服电机
#include <Servo.h>
// 数码管
#include "TM1637.h"
// 软件串口
#include <SoftwareSerial.h>

// 【端口定义】
// 0、1                     保留（供调试用）
// 2                        中断端口
// 3                        RFID
// 4、5、6                  按钮
// 7                        伺服电机（摆臂）
// 8、9、10、11、12、13     OLED显示屏
// A0、A1、A2               吸盘电机
// A3                       伺服电机（按钮）
// A4、A5                   语音控制模块

#define PIN_RFID_RX                     0
#define PIN_RFID_TX                     1
#define PIN_BUTTON_INTERRUPT            2   // INT0
#define PIN_BUTTON_START                4
#define PIN_BUTTON_PASSWORD_1           5
#define PIN_BUTTON_PASSWORD_2           6
#define PIN_SERVO_CAMERA_MOTION         7
#define PIN_SERVO_CAMERA_BUTTON         A3  // 原D8
#define PIN_OLED_RESET                  8
#define PIN_OLED_CTRL                   9
#define PIN_OLED_CS                     10
#define PIN_OLED_MOSI                   11  // SPI
#define PIN_OLED_MISO                   12  // SPI （未使用）
#define PIN_OLED_SCK                    13  // SPI
#define PIN_MOTOR_DIRECTION_1           A0  // 原D9
#define PIN_MOTOR_DIERCTION_2           A1  // 原D10
#define PIN_MOTOR_ENABLE                A2  // 原D11
#define PIN_RFID_TX_2                   3
#define PIN_RFID_RX_2                   12
#define PIN_TEST_LED1                   13  // 仅用于测试
#define PIN_TEST_LED2                   13  // 仅用于测试
#define PIN_NIXIETUBE_CLK               A0  // A0/D14（废弃）
#define PIN_NIXIETUBE_DIO               A1  // A1/D15（废弃）
#define PIN_SPEECH_RX                   A4
#define PIN_SPEECH_TX                   A5  // 未使用

// 【枚举定义】
enum APP_MODE_STATUS
{
  LOCKED = 0,                           // 
  INIT = 1,                             // 程序初始化、吸盘上锁
  WAIT_FOR_START = 2,                   // 等待按下“开始”按钮
  PREPARE_TO_WORK = 3,                  // 升起摄像头
  NORMAL = 4,                           // 正常工作（计时、计数、显示、拍照）
  OVER_WORK = 5,                        // 降下摄像头
  FINISH = 6,                           // 等待按下按钮
  PASSWORD = 7,                         // 输入密码
  UNLOCKED = 8,                         // 吸盘解锁
  OVER = 9,                             // 结束
  OTHER_STATUS = 9,
};

enum BUTTON_PRESSED_STATUS
{
  BUTTON_UNPRESSED = 0,
  BUTTON_PRESSED = 1,
};

enum BUTTON_PRESSED_TYPE
{
  NONE_BUTTON = 0,
  START_BUTTON = 1,
  STOP_BUTTON = 2,
  PASSWORD1_BUTTON = 3,
  PASSWORD2_BUTTON = 4,
};

// 【中断定义】
#define INTERRUPT_BUTTON                0 //(Pin2)

// 【设备定义】
// OLED显示屏
U8GLIB_NHD31OLED_GR device_oled(10,9,8);// 使用SPI硬件接口通讯 : (SCLK = 13, SDIN = 11,系统默认) CS = 10, DC = 9，RES = 8
// 伺服电机（摄像头动作）
Servo device_servo_motion;
// 伺服电机（摄像头按钮）
Servo device_servo_button;
// 数码管
//TM1637 device_nixietube(PIN_NIXIETUBE_CLK, PIN_NIXIETUBE_DIO);
// RFID读卡器（软件串口）
SoftwareSerial serial_rfid(PIN_RFID_RX_2, PIN_RFID_TX_2);
// 语音识别模块（软件串口）
SoftwareSerial serial_speech(PIN_SPEECH_RX, PIN_SPEECH_TX);


// 【设备参数定义】
// 伺服电机（摄像头动作）
#define DEVICE_SERVO_MOTION_ANGLE_INIT          5
#define DEVICE_SERVO_MOTION_ANGLE_WORKING       175
// 伺服电机（摄像头按钮）
#define DEVICE_SERVO_BUTTON_ANGLE_INIT          80
#define DEVICE_SERVO_BUTTON_ANGLE_WORKING       170

// 【变量定义】
// ----------------------------------------------------------------
APP_MODE_STATUS _appMode = INIT;
// 应用程序模式定义（已过时）
// 模式 0: 输入及显示密码
// 模式 1: 吸盘解锁
// 模式 2: 等待按下开始按钮
// 模式 3: 初始化计数、抬起摄像头
// 模式 4: 正常工作（计时、计数、显示、拍照）
// 模式 5: 结束计时、收回摄像头
// 模式 6: 程序结束（无动作执行，需重启以便重新工作）
// ----------------------------------------------------------------
// 按钮按下标志（默认为未按下按钮）
BUTTON_PRESSED_STATUS _buttonPressedFlag = BUTTON_UNPRESSED;
// 按钮按下类型标志（默认为空）
BUTTON_PRESSED_TYPE _buttonTypeFlag = NONE_BUTTON;
// ----------------------------------------------------------------
// 程序设置的密码
BUTTON_PRESSED_TYPE _password[4] = {PASSWORD1_BUTTON, PASSWORD2_BUTTON, PASSWORD1_BUTTON, PASSWORD1_BUTTON};  // 原代码：{PASSWORD1_BUTTON, PASSWORD1_BUTTON, PASSWORD1_BUTTON, PASSWORD1_BUTTON};
// 实际输入的密码
BUTTON_PRESSED_TYPE _inputPassword[4] = {NONE_BUTTON, NONE_BUTTON, NONE_BUTTON, NONE_BUTTON};
// 密码长度
int _passwordLength = 4;
// 当前输入密码的位置
int _currentInputPosition = 0;
// ----------------------------------------------------------------
// 开始计时时间
unsigned long _startTime = 0UL;
// 计数圈数
int _totalCount = 0;
// ----------------------------------------------------------------
// RFID读卡器读取的数据头格式
unsigned char _HEADER[4] = {0x00, 0x00, 0xE2, 0x00};
// ISO 18000-6C卡片1 ID
unsigned char _CARD_ID_1[12] = {0x41, 0x06, 0x22, 0x12, 0x01, 0x67, 0x26, 0x00, 0x10, 0x9C, 0x00, 0x69};
// ISO 18000-6C卡片2 ID
unsigned char _CARD_ID_2[12] = {0x51, 0x63, 0x43, 0x01, 0x01, 0x98, 0x22, 0x32, 0x22, 0x47, 0x00, 0xd1};
// 从串口读取数据的缓冲区
unsigned char _buffers[17];
// ----------------------------------------------------------------
// 最近一次接受到有效的RFID卡的时间
unsigned long _lastReceivedTagTime = 0UL;
// 两次接收RFID卡的时间间隔
unsigned long _tagReceivedInterval = 4000UL;
// ----------------------------------------------------------------

// 【初始化代码】
void setup()
{
  // ！端口初始化
  pinMode(PIN_BUTTON_START, INPUT);
  pinMode(PIN_BUTTON_PASSWORD_1, INPUT);
  pinMode(PIN_BUTTON_PASSWORD_2, INPUT);
  pinMode(PIN_MOTOR_DIRECTION_1, OUTPUT);
  pinMode(PIN_MOTOR_DIERCTION_2, OUTPUT);
  pinMode(PIN_MOTOR_ENABLE, OUTPUT);
  //pinMode(PIN_TEST_LED1, OUTPUT);
  //pinMode(PIN_TEST_LED2, OUTPUT);
  
  // ！中断初始化
  attachInterrupt(INTERRUPT_BUTTON, buttonPressed, RISING);
  
  // ！设备初始化
  // OLED显示屏
  device_oled.setColorIndex(3);
  device_oled.setFont(u8g_font_unifont);
  device_oled.firstPage();
  do
  {
  }
  while(device_oled.nextPage());
  // 数码管
  //device_nixietube.init();
  //device_nixietube.set(BRIGHT_TYPICAL);
  // 伺服电机（摄像头动作）
  device_servo_motion.attach(PIN_SERVO_CAMERA_MOTION);
  device_servo_motion.write(DEVICE_SERVO_MOTION_ANGLE_INIT);
  // 伺服电机（摄像头按钮）
  device_servo_button.attach(PIN_SERVO_CAMERA_BUTTON);
  device_servo_button.write(DEVICE_SERVO_BUTTON_ANGLE_INIT);
  // RFID读卡器
  Serial.begin(9600);     // 目前为调试用
  serial_rfid.begin(9600);
  // 语音识别模块
  serial_speech.begin(9600);
}

// 【主循环代码】
void loop()
{
  switch(_appMode)
  {
    case INIT:
      init_mode_loop();
      break;
    case WAIT_FOR_START:
      wait_mode_loop();
      break;
    case PREPARE_TO_WORK:
      prepare_mode_loop();
      break;
    case NORMAL:
      normal_mode_loop();
      break;
    case OVER_WORK:
      over_mode_loop();
      break;
    case FINISH:
      finish_mode_loop();
      break;
    case PASSWORD:
      password_mode_loop();
      break;
    case UNLOCKED:
      unlocked_mode_loop();
  }
}

// INIT 模式
void init_mode_loop()
{
  playWelcomeSpeech();
  device_oled.firstPage();
  do
  {
    device_oled.drawStr(2, 13, "Locking...");
  }
  while(device_oled.nextPage());

  // 吸盘上锁
  motorLock();

  device_oled.firstPage();
  do
  {
    device_oled.drawStr(2, 13, "Done !");
  }
  while(device_oled.nextPage());

  delay(2000);

  device_oled.firstPage();
  do
  {
    device_oled.drawStr(2, 13, "Press button to start.");
  }
  while(device_oled.nextPage());
  
  clearButtonFlag();
  clearSpeechSerialBuffers();

  _appMode = WAIT_FOR_START;
}

// WAIT_FOR_START 模式
void wait_mode_loop()
{
  // 强行设置的按钮状态
  //_buttonPressedFlag = BUTTON_PRESSED;
  //_buttonTypeFlag = START_BUTTON;
  //delay(1000);
  // 注意：以上代码仅在调试时使用

  readSpeechCommand();
  
  if(_buttonPressedFlag == BUTTON_PRESSED && isStartButtonPressed(_buttonTypeFlag))
  {
    clearButtonFlag();
    
    printCountAndTime(0, 0UL);

    _appMode = PREPARE_TO_WORK;
  }
}

// PREPARE_TO_WORK 模式
void prepare_mode_loop()
{
  // RFID读卡器
  //Serial.begin(9600);
  clearRfidSerialBuffers();
  
  // 伺服电机（摆臂）
  device_servo_motion.write(DEVICE_SERVO_MOTION_ANGLE_WORKING);
  
  // 计数器
  _startTime = millis();
  _lastReceivedTagTime = _startTime;
  //nixietubeDisplayNumber(_totalCount, false);

  _appMode = NORMAL;
}

// NORMAL 模式
void normal_mode_loop()
{
  //digitalWrite(PIN_TEST_LED2, HIGH);

  readSpeechCommand();

  if(_buttonPressedFlag == BUTTON_PRESSED && isStopButtonPressed(_buttonTypeFlag))
  {
    _appMode = OVER_WORK;
    clearButtonFlag();
  }
  
  if(serial_rfid.available())
  {
    leftShift();
    _buffers[16] = serial_rfid.read();
  }
  else
  {
   // return;   // 调试时临时注释掉，此处应该有return！
  }

  if(1)   // 原始代码：if(isHeader() && isCard1())
  {
    unsigned long currentMillis = millis();
    if(currentMillis - _lastReceivedTagTime < _tagReceivedInterval)
      return;
    _totalCount++;

    //nixietubeDisplayNumber(_totalCount, false);
    printCountAndTime(_totalCount, currentMillis - _startTime);
    _lastReceivedTagTime = currentMillis;

    // 伺服电机（按钮）
    device_servo_button.write(DEVICE_SERVO_BUTTON_ANGLE_WORKING);
    delay(400);
    device_servo_button.write(DEVICE_SERVO_BUTTON_ANGLE_INIT);

    delay(20);
    clearRfidSerialBuffers();
    clearBuffers();

    // 伺服电机代码

    if(_totalCount > 9999)
      _totalCount = 0;
  }
}

// OVER_WORK 模式
void over_mode_loop()
{
  // RFID读卡器（软件串口）
  serial_rfid.end();
  // 数码管
  //nixietubeDisplayNumber(_totalCount, true);
  // 伺服电机（摆臂）
  device_servo_motion.write(DEVICE_SERVO_MOTION_ANGLE_INIT);

  // 强行设置的按钮状态
  _buttonPressedFlag = BUTTON_PRESSED;
  _buttonTypeFlag = START_BUTTON;
  delay(1000);
  // 注意：以上代码仅在调试时使用
  
  if(_buttonPressedFlag == BUTTON_PRESSED && isStartButtonPressed(_buttonTypeFlag))
  {
    _appMode = FINISH;
  }
}

// FINISH 模式
void finish_mode_loop()
{
  drawPasswordPage();

  _appMode = PASSWORD;
}

// PASSWORD 模式
void password_mode_loop()
{
  readSpeechCommand();
  
  if(_buttonPressedFlag == BUTTON_PRESSED)
  {
    if(isPasswordButtonPressed(_buttonTypeFlag))
    {
      if(_currentInputPosition < _passwordLength)
      {
        _inputPassword[_currentInputPosition] = _buttonTypeFlag;
        _currentInputPosition++;
        drawPasswordPage();
      }
      
      if(_currentInputPosition == _passwordLength)
      {
        if(isCorrectPassword())
        {
          device_oled.firstPage();
          do
          {
            device_oled.drawStr(2, 13, "Success !");
          }
          while(device_oled.nextPage());
          delay(1000);
          _appMode = UNLOCKED;
        }
        _currentInputPosition = 0;
        clearInputPassword();
      }
    }
  
    clearButtonFlag();
  }
}

// UNLOCKED 模式
void unlocked_mode_loop()
{
  device_oled.firstPage();
  do
  {
    device_oled.drawStr(2, 13, "Unlocking...");
  }
  while(device_oled.nextPage());
  
  // 吸盘解锁
  motorUnlock();

  playFinishSpeech();
  device_oled.firstPage();
  do
  {
    device_oled.drawStr(2, 13, "FINISH.");
  }
  while(device_oled.nextPage());

  _appMode = OVER;
}

// 【按钮代码】
void buttonPressed()
{
  _buttonPressedFlag = BUTTON_PRESSED;

  _buttonTypeFlag = NONE_BUTTON;
  if(digitalRead(PIN_BUTTON_START) == HIGH)
    _buttonTypeFlag = START_BUTTON;
  if(digitalRead(PIN_BUTTON_PASSWORD_1) == HIGH)
    _buttonTypeFlag = PASSWORD1_BUTTON;
  if(digitalRead(PIN_BUTTON_PASSWORD_2) == HIGH)
    _buttonTypeFlag = PASSWORD2_BUTTON;
}

boolean isPasswordButtonPressed(int buttonType)
{
  if(buttonType == PASSWORD1_BUTTON || buttonType == PASSWORD2_BUTTON)
    return true;

  return false;
}

boolean isStartButtonPressed(int buttonType)
{
  if(buttonType == START_BUTTON)
    return true;

  return false;
}

boolean isStopButtonPressed(int buttonType)
{
  if(buttonType == STOP_BUTTON)
    return true;

  return false;
}

void clearButtonFlag()
{
  _buttonPressedFlag = BUTTON_UNPRESSED;
  _buttonTypeFlag = NONE_BUTTON;
}

// 【密码代码】
boolean isCorrectPassword()
{
  for(int i = 0; i < _passwordLength; i++)
  {
    if(_inputPassword[i] != _password[i])
      return false;
  }

  return true;
}

void clearInputPassword()
{
  for(int i = 0; i < _passwordLength; i++)
  {
    _inputPassword[i] = NONE_BUTTON;
  }
}

// 【RFID读卡器代码】
void leftShift()
{
  for(int i = 0; i < 16; i++)
  {
    _buffers[i] = _buffers[i + 1];
  }
}

boolean isHeader()
{
  for(int i = 0; i < 4; i++)
  {
    if(_buffers[i] != _HEADER[i])
      return false;
  }
  return true;
}

boolean isCard1()
{
  for(int k = 0; k < 12; k++)
  {
    if(_buffers[k + 4] != _CARD_ID_1[k])
    {
      return false;
    }
  }
  return true;
}

void clearBuffers()
{
  for(int i = 0; i < 17; i++)
  {
    _buffers[i] = 0x00;
  }
}

void clearRfidSerialBuffers()
{
  while(serial_rfid.available())
    serial_rfid.read();
}

// 【LCD代码】
void printCountAndTime(int count, unsigned long time)
{
  unsigned long totalSeconds = time / 1000UL;
  int seconds = totalSeconds % 60;
  int minutes = (totalSeconds - seconds) % 3600 / 60;
  //int hours = (totalSeconds - minutes * 60 - seconds) / 3600;
  
  device_oled.firstPage();
  do
  {
    // 绘制圈数文字
    device_oled.drawStr(2, 13, "Count:");
    device_oled.setPrintPos(50, 13);
    device_oled.print(count);
    // 绘制时间文字
    device_oled.drawStr(2, 29, "Time:");
    device_oled.setPrintPos(50, 29);
    if(minutes < 10)
      device_oled.print("0");
    device_oled.print(minutes);
    device_oled.print(":");
    if(seconds < 10)
      device_oled.print("0");
    device_oled.print(seconds);
  }
  while(device_oled.nextPage());
  
  //int hours = (int)(totalSeconds / 3600UL);
}

// 【数码管代码】
void nixietubeDisplayNumber(int number, bool displayAllBit)
{
  int num1 = number % 10;
  int num2, num3, num4;
  if(displayAllBit)
  {
    // 四位全部显示代码
    num2 = (number / 10) % 10;
    num3 = (number / 100) % 10;
    num4 = (number / 1000) % 10;
  }
  else
  {
    // 仅显示存在位代码
    num2 = number < 10 ? 0x7F : (number / 10) % 10;
    num3 = number < 100 ? 0x7F : (number / 100) % 10;
    num4 = number < 1000 ? 0x7F : (number / 1000) % 10;
  }


  //device_nixietube.display(0, num4);
  //device_nixietube.display(1, num3);
  //device_nixietube.display(2, num2);
  //device_nixietube.display(3, num1);
}

// 【OLED显示屏代码】
void drawPasswordPage()
{
  device_oled.firstPage();
  do
  {
    device_oled.drawStr(2, 13, "Password...");
    device_oled.setPrintPos(2, 29);
    for(int i = 0; i < _currentInputPosition; i++)
      device_oled.print('*');
    for(int j = 0; j < _passwordLength - _currentInputPosition; j++)
      device_oled.print('_');
  }
  while(device_oled.nextPage());
}

// 【语音识别模块代码】
// @WriteKeywords#开始 001|停止 002|衣 003|二 004|$
// @WriteFlashText#|001 |002 $
void readSpeechCommand()
{
  if(serial_speech.available())
  {
    int command = serial_speech.read();
    if(command == 0x01) // 说“开始”
    {
      _buttonPressedFlag = BUTTON_PRESSED;
      _buttonTypeFlag = START_BUTTON;
    }
    else if(command == 0x02)  // 说“停止”
    {
      _buttonPressedFlag = BUTTON_PRESSED;
      _buttonTypeFlag = STOP_BUTTON;
    }
    else if(command == 0x03)  // 说“一”
    {
      _buttonPressedFlag = BUTTON_PRESSED;
      _buttonTypeFlag = PASSWORD1_BUTTON;
    }
    else if(command == 0x04)  // 说“二”
    {
      _buttonPressedFlag = BUTTON_PRESSED;
      _buttonTypeFlag = PASSWORD2_BUTTON;
    }
  }
}

void clearSpeechSerialBuffers()
{
  while(serial_speech.available())
    serial_speech.read();
}

void inline playWelcomeSpeech()
{
  serial_speech.write("@TextToSpeech#\xBB\xB6\xD3\xAD\xCA\xB9\xD3\xC3$");
}

void inline playFinishSpeech()
{
  serial_speech.write("@TextToSpeech#\xD4\xD9\xBC\xFB$");
}

// 【吸盘电机代码】
void motorLock()
{
  digitalWrite(PIN_MOTOR_DIRECTION_1, HIGH);
  digitalWrite(PIN_MOTOR_DIERCTION_2, LOW);
  digitalWrite(PIN_MOTOR_ENABLE, HIGH);
  delay(6000);
  motorStop();
}

void motorUnlock()
{
  digitalWrite(PIN_MOTOR_DIRECTION_1, LOW);
  digitalWrite(PIN_MOTOR_DIERCTION_2, HIGH);
  digitalWrite(PIN_MOTOR_ENABLE, HIGH);
  delay(2500);
  motorStop();
}

void motorStop()
{
  digitalWrite(PIN_MOTOR_DIRECTION_1, LOW);
  digitalWrite(PIN_MOTOR_DIERCTION_2, LOW);
  digitalWrite(PIN_MOTOR_ENABLE, LOW);
}

