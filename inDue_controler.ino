/**
   加载到单片机 Due 上的控制程序
   实现四个控制分流方式：平时状态传送角度，按键A传送指令加角度，按键B传送指令加6轴数据，按键C校准一次
   实现多个交互方式：平时状态通过角度数据进行持续交互，通过按键A进行持续指令交互，通过按键B进行单指令交互
*/


/**
   硬件引脚
*/
#define GEST_RX 19 //JY61 PIN-TX & DUE PIN-RX   Serial1
#define GEST_TX 18 //JY61 PIN-RX & DUE PIN-TX   Serial1
#define BT_A 35
#define BT_B 37
#define BT_C 39
#define BLUE_RX = 17 //BLE PIN-TX & DUE PIN-RX   Serial2
#define BLUE_TX = 16 //BLE PIN-RX & DUE PIN-TX   Serial2


/**
   数据变量
*/
char state = -1; //当前指令状态：-1, H, A, B, C ?
//预留400的空间，因为不知道JY61传来的数据有多少个，先用400存下来
float data[6][400];
float angle[3] = {0};
unsigned char Re_buf[11], counter = 0; //存储从JY61读到的数据包，counter指示读到数据包的第几位
unsigned char sign = 0; //表示信号时刻，即data存储到了第几行


/**
   通用函数
*/
//初始化data数组
void initial() {
  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 256; j++) {
      data[i][j] = 0;
    }
  }
  sign = 0;
}

// 返回哪个按钮被按下
char get_button() {
  if (digitalRead(BT_A) == HIGH) {
    return 'A';
  } else if (digitalRead(BT_B) == HIGH) {
    return 'B';
  } else if (digitalRead(BT_C) == HIGH) {
    return 'C';
  } else {
    return -1;
  }
}

// 角度数据读取，这个按钮B状态不能用，另见函数detect_and_store
void detect() {
  while (true) {
    if (Serial1.available()) {
      Re_buf[counter] = (unsigned char)Serial1.read();
      if (counter == 0 && Re_buf[0] != 0x55) continue; //第0号数据不是帧头
      counter++;
      if (counter == 11)          //接收到11个数据
      {
        counter = 0;           //重新赋值，准备下一帧数据的接收
        if (Re_buf[0] == 0x55 && Re_buf[1] == 0x53)   //检查帧头以及角度包
        {
          angle[0] = (short(Re_buf[3] << 8 | Re_buf[2])) / 32768.0 * 180;
          angle[1] = (short(Re_buf[5] << 8 | Re_buf[4])) / 32768.0 * 180;
          angle[2] = (short(Re_buf[7] << 8 | Re_buf[6])) / 32768.0 * 180;
          break;
        }
      }
    }
  }
}

//JY61模块：z轴校准和加计校准
void calibrate() {
  //特别注意：在执行此初始化时候，要保证JY61：x轴指向竖直向下，y轴指向水平向左，z轴指向水平向前！！！
  //z轴归零
  byte zzero[3] = {0xFF, 0xAA, 0x52};
  for (int i = 0; i < 3; i++) {
    Serial1.write(zzero[i]);
  } for (int i = 0; i < 3; i++) {
    Serial1.write(zzero[i]);
  }
  Serial.println("z-zeroing");

  //加计校准
  byte acheck[3] = {0xFF, 0xAA, 0x67};
  for (int i = 0; i < 3; i++) {
    Serial1.write(acheck[i]);
  } for (int i = 0; i < 3; i++) {
    Serial1.write(acheck[i]);
  }
  Serial.println("A-calibration");
}

//发送字符'H'至蓝牙
//todo @author Louise, Simon, Marc-Antoine
bool send_H() {
 return true;
}


/**
   开机操作：初始化配置，初始化数据，传感器校准等
*/
void setup() {
  Serial.begin(9600);

  Serial.println("initializing...");

  //设置JY61初始波特率为 115200
  Serial1.begin(115200);
  Serial.println("baud rate：115200");

  //向JY61发送一次指令，确保其为115200的波特率
  byte baud[3] = {0xFF, 0xAA, 0x63};
  for (int i = 0; i < 3; i++) {
    Serial1.write(baud[i]);
  }
  Serial.println("baud rate 115200, return rate 100Hz.");
  Serial1.begin(115200);

  //下面是校准
  calibrate();

  //初始化data数组
  initial();

  //设置按钮
  pinMode(BT_A, INPUT);
  pinMode(BT_B, INPUT);
  pinMode(BT_C, INPUT);
}


/**
   平时状态函数系列
*/
//todo @author Louise, Simon, Marc-Antoine
//将angle数组发送到蓝牙
bool send_angles_to_ble() {
  return true;
}


/**
   按钮A函数系列
*/
//发送字符'A'和angles数组到蓝牙
//todo @author Louise, Simon, Marc-Antoine
bool send_A_to_ble() {
  return true;
}


/**
   按钮 B 系列操作函数
   @return
*/
// 由于JY61传数据时是一个包一个包的传送，一个包仅仅包含三个轴加速度或者三个轴角速度，因此加速度和角速度两类数据一次解包只能获取一类
// 这个函数用来判断data数组的第sign行即[6][sign]是否为完整的六列数据
// 判断方法是，如果出现多于三列数据值为0(初始值)，则是不完整数据（原则上，这并不是个完整的判断方法，但是足够用在咱们这个项目当中）
bool a_tuple() {
  int counter = 0;
  for (int i = 0; i < 6; i++) {
    if (data[i][sign] == 0) {
      counter++;
    }
  }

  if (counter >= 3) {
    return false;
  } else {
    return true;
  }
}

/*
  //在Serial中打印数据
  void print_data() {
    for (int j = 0; j < 256; j++) {
        for (int i = 0; i < 6; i++) {
            Serial.print(data[i][j]);
            Serial.print("  ");
        }
        Serial.println();
    }
    Serial.println("===============================");
  }
*/

// 检测并读取JY61的数据，将读取到的数据存储在data数组当中
void detect_and_store() {
  while (digitalRead(BT_A) == HIGH) {
    if (Serial1.available()) {
      Re_buf[counter] = (unsigned char)Serial1.read();
      if (counter == 0 && Re_buf[0] != 0x55) continue; //第0号数据不是帧头
      counter++;
      if (counter == 11)          //接收到11个数据
      {
        counter = 0;           //重新赋值，准备下一帧数据的接收
        if (Re_buf[0] == 0x55)   //检查帧头
        {
          switch (Re_buf [1])
          {
            case 0x51:
              data[0][sign] = (short(Re_buf [3] << 8 | Re_buf [2])) / 32768.0 * 16;
              data[1][sign] = (short(Re_buf [5] << 8 | Re_buf [4])) / 32768.0 * 16;
              data[2][sign] = (short(Re_buf [7] << 8 | Re_buf [6])) / 32768.0 * 16;
              if (a_tuple()) {
                sign++;
                if (sign >= 256) {
                  return;
                }
              }
              break;
            case 0x53:
              data[3][sign] = (short(Re_buf [3] << 8 | Re_buf [2])) / 32768.0 * 180;
              data[4][sign] = (short(Re_buf [5] << 8 | Re_buf [4])) / 32768.0 * 180;
              data[5][sign] = (short(Re_buf [7] << 8 | Re_buf [6])) / 32768.0 * 180;
              if (a_tuple()) {
                sign++;
                if (sign >= 256) {
                  return;
                }
              }
              break;
          }
        }
      }
    }
  }
}

//发送字符'B'和data数组到蓝牙
//todo @author Louise, Simon, Marc-Antoine
bool send_B_to_ble() {
  return true;
}


/**
 * 主循环体
 */
void loop() {
  //    if () {
  //        Serial.println("Button pushed.");
  //        detect_and_store();
  //        print_data();
  //        initial();
  //    }
  switch (get_button()) {

    case 'A':
      Serial.println("Button A gotten.");
      state = 'A';
      detect();
      if (send_A_to_ble()) {
        Serial.println("A_to_BLE succeeds.");
      } else {
        Serial.println("A_to_BLE failed.");
      }
      break;

    case 'B':
      Serial.println("Button B gotten.");
      state = 'B';
      if (send_H()) {
        Serial.println("H sent.");
      } else {
        Serial.println("H sending failed.");
      }
      detect_and_store();
      if (send_B_to_ble()) {
        Serial.println("B_to_BLE succeeds.");
      } else {
        Serial.println("B_to_BLE  failed.");
      }
      initial();
      break;

    case 'C':
      Serial.println("Button C gotten.");
      state = 'C';
      while (digitalRead(BT_C) == HIGH); //按下C按钮时，程序挂起
      calibrate();
      if (send_H()) {
        Serial.println("H sent.");
      } else {
        Serial.println("H sending failed.");
      }
      break;

    default:
      Serial.println("No Button.");
      state = -1;
      detect();
      if (send_angles_to_ble()) {
        Serial.println("Angle to BLE succeeds.");
      } else {
        Serial.println("Angle to BLE failed.");
      }
      break;
  }
}
