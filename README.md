# inDue Controler
in service for project ZenS of the organization CRI.


#### Workflow
1. 开机，setup () { 初始化配置，初始化数据，传感器校准等 }。  (保证JY61：x轴指向竖直向下，y轴指向水平向左，z轴指向水平向前)
2. 控制分流：
    - 未按下按钮：平时状态，通过蓝牙**仅**发送三轴角度数据
    -  按下按钮A：通过蓝牙发送 字符‘A’(作为指令字符)和三轴角度数据
    -  放开按钮A：回到平时状态
    - 按下按钮B：通过蓝牙发送 字符'H'(作为指令字符)，然后开始记录六轴数据
    - 放开按钮B: 通过蓝牙发送 字符'B'(作为指令字符)和六轴数据数组(\[6, 400\])，然后回到平时状态
    - 按下按钮C: 此时挂起单片机程序，保证JY61处于上述校准位置(并保持至少4秒)
    - 放开按钮C: 向JY61发送校准指令，然后通过蓝牙发送 字符'H'(作为指令字符), 然后回到平时状态
3. 关机，即关闭充放电一体化模块的供电电源，结束程序

<br>
\*可能会遇到很多延时不到位的问题，需要在具体试验的过程中
调整inDue_controler和host的延时，以协调两者的信息交换 

#### Structure
- inDue_controler.ino: run in Arduino Due. There aren't other code files for simplicity and effectiveness.
- README.md