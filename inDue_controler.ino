/**
   加载到单片机 Due 上的控制程序
   实现四个控制分流方式：平时状态传送角度，按键A传送指令加角度，按键B传送指令加6轴数据，按键C校准一次
   实现多个交互方式：平时状态通过角度数据进行持续交互，通过按键A进行持续指令交互，通过按键B进行单指令交互
*/


/**
   硬件引脚
*/

//todo: 改变引脚
#define BT_A 35
#define BT_B 37
#define BT_C 39
#define BT_D 47
#define BLUE_RX 19 //BLE PIN-TX & DUE PIN-RX   Serial1
#define BLUE_TX 18 //BLE PIN-RX & DUE PIN-TX   Serial1
#define GEST_RX 17 //JY61 PIN-TX & DUE PIN-RX   Serial2
#define GEST_TX 16 //JY61 PIN-RX & DUE PIN-TX   Serial2
#define DATA_SIZE (4*240)
/**
   数据变量
*/

char state = 'P'; //当前指令状态：A, B,C, P?
//预留400的空间，因为不知道JY61传来的数据有多少个，先用400存下来
float data[6][400];
float angle[36] = {0};//一次记录36个发送
unsigned char Re_buf[11], counter = 0; //存储从JY61读到的数据包，counter指示读到数据包的第几位
unsigned int sign = 0; //表示信号时刻，即data存储到了第几行
//这里可以把datasize改为240来提高速度
 //通过蓝牙单次发送的最大字节数
//这里是用于transform的union，union的size设置为足够大，实例化一个trans_union，全程用它来转换，不管是三轴还是六轴数据
union transform {
  char ch[6 * 400 * sizeof(float)];
  float fl[6 * 400];
} trans_union; //用于待发送数据的格式转换

//这里用于标注在loop间发送六轴数据的批数，每次发完+1
int fois = 0;//用于标注发送数据批数

/**
   通用函数
*/
//初始化data数组
void initial() {
  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 400; j++) {
      data[i][j] = 0;
    }
  }
  sign = 0;
}

//将trans_union清空
void initialize_union(union transform &trans_union) {
  for (int i = 0; i < 6 * 400; i++) {
    trans_union.fl[i] = 0.0;
  }
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
  int batch = 0;
  while (true) {
    if (Serial2.available()) {
      Re_buf[counter] = (unsigned char)Serial2.read();
      if (counter == 0 && Re_buf[0] != 0x55) continue; //第0号数据不是帧头
      counter++;
      if (counter == 11)          //接收到11个数据
      {
        counter = 0;           //重新赋值，准备下一帧数据的接收
        if (Re_buf[0] == 0x55 && Re_buf[1] == 0x53)   //检查帧头以及角度包
        {
          angle[batch * 3 + 0] = (short(Re_buf[3] << 8 | Re_buf[2])) / 32768.0 * 180;
          angle[batch * 3 + 1] = (short(Re_buf[5] << 8 | Re_buf[4])) / 32768.0 * 180;
          angle[batch * 3 + 2] = (short(Re_buf[7] << 8 | Re_buf[6])) / 32768.0 * 180;
          //                    break;
          batch++;
          if (batch == 12) {
            break;
          }
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
    Serial2.write(zzero[i]);
  } for (int i = 0; i < 3; i++) {
    Serial2.write(zzero[i]);
  }
  Serial.println("z-zeroing");

  //加计校准
  byte acheck[3] = {0xFF, 0xAA, 0x67};
  for (int i = 0; i < 3; i++) {
    Serial2.write(acheck[i]);
  } for (int i = 0; i < 3; i++) {
    Serial2.write(acheck[i]);
  }
  Serial.println("A-calibration");
}

//新增函数，需要配合float_to_union 系列函数把float写到trans_union里。注意dimension为6的时候自动根据fois来发送对应的批次
//发送数据到蓝牙,@dimension为待发送数据维数
bool send_data(transform &data, int dimension) {
  if (dimension == 3) {
    for (int i = 0; i < 36 * sizeof(float); i++) {
      Serial1.write(data.ch[i]);
    }

  } //以char格式发送数据
  if (dimension == 6) {
    for (int k = 0; k < DATA_SIZE; k++) {
      Serial1.write(data.ch[k]);
    }
  }//以char格式发送数据
  delay(1000);// todo 通过这个来加速
  return true;
}

//把angle写到trans_union里
//接受angle数组和trans_union作为参数，angle数组中的数据复制到trans_union里准备发送
bool data_to_union_3(union transform &trans_union, float angle[36]) {
  initialize_union(trans_union);//可能并无必要，因为trans_union的单元可能存着原来的数据，比如用angles覆盖data时候，有一部分没有覆盖
  for (int i = 0; i < 36; i++) {
    trans_union.fl[i] = angle[i];
  }
  return true;
}

/**
   开机操作：初始化配置，初始化数据，传感器校准等
*/
void setup() {
  Serial.begin(9600);

  Serial.println("initializing...");

  //设置JY61初始波特率为 115200
  Serial2.begin(115200);
  Serial.println("baud rate of JY61：115200");

  //todo：初始化hc-05
  //设置HC-05初始波特率为38400
  Serial1.begin(38400);
  Serial.println("baud rate of HC-05：38400");

  //向JY61发送一次指令，确保其为115200的波特率
  byte baud[3] = {0xFF, 0xAA, 0x63};
  for (int i = 0; i < 3; i++) {
    Serial2.write(baud[i]);
  }
  Serial.println("baud rate 115200, return rate 100Hz.");
  Serial2.begin(115200);

  //下面是校准
  calibrate();

  //初始化data数组
  initial();

  //设置按钮
  pinMode(BT_A, INPUT);
  pinMode(BT_B, INPUT);
  pinMode(BT_C, INPUT);
  pinMode(BT_D, INPUT);
}



/**
   按钮A函数系列
*/
//todo：新增函数，注意要提前write一个A进去
//发送字符'A'和angles数组到蓝牙
bool send_A_to_ble() {
  return send_data(trans_union, 3);
}


/**
   按钮 B 系列操作函数
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

//把data写到trans_union里
//接受data数组和trans_union作为参数，将data数组中的数据复制到trans_union里准备发送
bool data_to_union_6(union transform &trans_union, float data[6][400]) {
  initialize_union(trans_union); //可能并无必要，因为trans_union的单元可能存着原来的数据，比如用angles覆盖data时候，有一部分没有覆盖
//  for (int i = 0; i < 6; i++) {
//    for (int j = 0; j < 400; j++) {
//      trans_union.fl[i * 400 + j] = data[i][j];
//    }
//  }
  for (int i = 0; i < DATA_SIZE/sizeof(float); i++) {
    trans_union.fl[i] = data[(fois*DATA_SIZE/sizeof(float)+i)/400][(fois*DATA_SIZE/sizeof(float)+i)%400];
  }
  return true;
}

// 检测并读取JY61的数据，将读取到的数据存储在data数组当中
void detect_and_store() {
  initial();
  while (digitalRead(BT_B) == HIGH) {
    if (Serial2.available()) {
      Re_buf[counter] = (unsigned char)Serial2.read();
      if (counter == 0 && Re_buf[0] != 0x55) continue; //第0号数据不是帧头
      counter++;
      if (counter == 11)          //接收到11个数据
      {
        counter = 0;           //重新赋值，准备下一帧数据的接收
        if (Re_buf[0] == 0x55)   //检查帧头
        {
//          int cter = 0;
          switch (Re_buf [1])
          {
            case 0x51:
              data[0][sign] = (short(Re_buf [3] << 8 | Re_buf [2])) / 32768.0 * 16;
              data[1][sign] = (short(Re_buf [5] << 8 | Re_buf [4])) / 32768.0 * 16;
              data[2][sign] = (short(Re_buf [7] << 8 | Re_buf [6])) / 32768.0 * 16;
              if (a_tuple()) {
                  sign++;
                  if (sign >= 400) {//todo 注意这里有个长度控制
                      return;
                  }
              }
/*
//              cter = 0;
//              for (int i = 0; i < 6; i++) {
//                if (data[i][sign] == 0) {
//                  cter++;
//                }
//              }
//
//              if (cter >= 3) {
//                
//              } else {
//                sign++;
//                if(sign>=400){
//                  return;
//                }
//              }
*/
              break;
            case 0x53:
              data[3][sign] = (short(Re_buf [3] << 8 | Re_buf [2])) / 32768.0 * 180;
              data[4][sign] = (short(Re_buf [5] << 8 | Re_buf [4])) / 32768.0 * 180;
              data[5][sign] = (short(Re_buf [7] << 8 | Re_buf [6])) / 32768.0 * 180;
              if (a_tuple()) {
                sign++;
                if (sign >= 400) {//todo 注意这里有个长度控制
                  return;
                }
              }
/*
//              cter = 0;
//              for (int i = 0; i < 6; i++) {
//                if (data[i][sign] == 0) {
//                  cter++;
//                }
//              }
//
//              if (cter >= 3) {
//                
//              } else {
//                sign++;
//                if(sign>=400){
//                  return;
//                }
//              }
*/
              break;
          }
        }
      }
    }
  }
//  for(int j = 0; j<400; j++){
//    Serial.print(data[0][j]); Serial.print(" "); Serial.print(data[1][j]); Serial.print(" "); Serial.print(data[2][j]); Serial.print(" "); Serial.print(data[3][j]); Serial.print(" "); Serial.print(data[4][j]); Serial.print(" "); Serial.println(data[5][j]);
//  }
}

//新增函数，注意这里只管发送数据，发送B的任务在loop函数里单独实现
//发送字符'B'和data数组到蓝牙
//todo 这个需要着重处理一下，保证一个函数发完全部数据，即分批次发，发完跳出这个while循环
bool send_B_to_ble() {
//  if(fois == 18){
//    for(int i = 0; i<36*4; i++){
//      Serial1.write(char(i));
//    }
//    delay(1000);
//    return true;
//  }
  return send_data(trans_union, 6);
}

//B功能的操作控制手柄
bool run_B_handler() {
  bool is_successful = true;
  
  while (state == 'B') { //注意，数据的发送是在放开B按钮之后进行的
    Serial1.write('B');
//    if(fois==18){//因为不知道为什么每次传到18就出错，而且是"18"这个数字本身有问题，所以这是传去100，发现数据内容不变，但是不会出现问题了..
//      Serial1.write(char(int(100)));
//    }else{
//      Serial1.write(char(fois));  
//    }
    Serial1.write(char(fois));  
    data_to_union_6(trans_union, data);
//    for(int i = 0;i<DATA_SIZE/sizeof(float); i++){
//      Serial.println(trans_union.fl[i]);
//    }
    // for (int k = 0; k < DATA_SIZE; k++) {
    //   Serial1.write(trans_union.ch[fois * DATA_SIZE + k]);
    // }
//    delay(2000);
    
   if (send_B_to_ble()) {
      Serial.print("B_to_BLE succeeds. Batch number:");
      Serial.println(fois);
      fois++;//todo：借此实现在下一个loop中发送下一批
   } else {
     is_successful = false;
     Serial.print("B_to_BLE  failed. Batch number:");
     Serial.println(fois);
   }
    if (fois == 4 * 400 * 6 / DATA_SIZE + 1) { //todo：这里注意要+1因为最后一批发完fois被++了
      state = 'P';
      fois = 0;
//      Serial.println("placeholder");
      delay(4000);//如果T莫名消失，那么原因是：这个延时不够，T附到了前一个buf的最后
      Serial1.write('T');//表示data的各批次都传完了
      delay(1000);
    }//最后一次要把fois归零，state重新设置为P
     delay(100);//todo 在此行暂停下程序，等待zens_host处理完一个buf
  }
  return is_successful;
}


/**
  平时状态系列函数
*/
bool send_angles_to_ble(transform &trans_union, float angle[36]) {
  return send_data(trans_union, 3);
}


/**
   主循环体
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
      Serial1.write('A');
      if (data_to_union_3(trans_union, angle)) {
        send_A_to_ble();
        Serial.println("A_to_BLE succeeds.");
      } else {
        Serial.println("A_to_BLE failed.");
      }
      break;

    case 'B':
      //todo：修改分支，把发送数据和initial的任务放到后边，其他没变
      Serial.println("Button B gotten.");
      state = 'B';
      // //todo 这里sendh删掉，换成下面的serial1.write('b'),不然要在每个batch里都发个h，并且修改cpp文件
      // if (send_H()) {
      //     Serial.println("H sent.");
      // } else {
      //     Serial.println("H sending failed.");
      // }
      detect_and_store(); // todo 此时zens_host收不到任何数据，应执行默认默认'P'情形
//      if (data_to_union_6(trans_union, data)) { //转码只需一次就好，即一次将data的全部数据转码，之后无需重复转码
//        // Serial1.write('B');//在handler中用这个操作控制批次接收的重启
//        if (run_B_handler()) { //B的发送通过这个操作柄循环控制
//          Serial.println("B sent.");
//        } else {
//          Serial.println("B sending failed.");
//        }
//
//      } else {
//        Serial.println("B-Data transcoding failed.");
//      }
        if (run_B_handler()) { //B的发送通过这个操作柄循环控制
          Serial.println("B sent.");
        } else {
          Serial.println("B sending failed.");
        }
      break;

    case 'C':
      Serial.println("Button C gotten.");
      delay(1000);
      state = 'C';
      Serial1.write('C');
      Serial.println("C sent.");
      while (digitalRead(BT_C) == HIGH); //按下C按钮时，程序挂起
      calibrate();
      break;

    default:
      Serial.println("No Button.");
      state = 'P';
      detect();// todo 此时若zens_host收不到任何数据，应执行默认默认'P'情形
      Serial1.write('P');
      if (data_to_union_3(trans_union, angle)) {
        send_angles_to_ble(trans_union, angle);
        Serial.println("Angle to BLE succeeds.");
      } else {
        Serial.println("Angle to BLE failed.");
      }
      break;
  }
}
