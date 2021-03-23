# 导入界面设计文件
from tkinter.constants import FALSE, TRUE
from PySide2.QtWidgets import QApplication, QMessageBox
from PySide2.QtUiTools import QUiLoader
from PySide2.QtGui import  QIcon
# 导入串口库
import serial
import serial.tools.list_ports
# 导入线程库
from threading import Thread
#导入时间相关的库
from time import sleep
# 定义遥控界面的类
class remotor:
    #初始化函数，必要框架的初始值设定
    def __init__(self):
        # 导入设计的框架
        self.ui = QUiLoader().load('remotor.ui')
        # 创建一个串口
        self.ser = serial.Serial()
        self.ui.open_port.clicked.connect(self.open_port)
        self.ui.close_port.clicked.connect(self.close_port)
        # 创建一个空线程，在发送命令时使用
        self.thread=Thread()
        self.port_str_list = []  # 用来存储切割好的串口号
        self.update_flag = True
        self.ui.update_button.clicked.connect(self.update_ports)
        # self.update = Thread(target=self.update_ports,args=())
        # self.update.start()
        # 设置发送遥控数据指令标志位，用于终止发送指令的线程
        self.send_flag=True
        #遥控器解锁/上锁
        self.ui.lock.clicked.connect(self.lock)
        #更改飞行模式
        self.ui.mode.currentIndexChanged.connect(self.change_mode)
        #更改控制模式
        self.ui.control.currentIndexChanged.connect(self.change_control)
        #一键起飞/降落
        self.ui.fly_land.clicked.connect(self.fly_land)
        #紧急制动
        self.ui.shut_down.clicked.connect(self.shutdown)
        #前后左右上下飞行
        self.ui.forward.pressed.connect(self.forward)
        self.ui.forward.released.connect(self.over)

        self.ui.back.pressed.connect(self.back)
        self.ui.back.released.connect(self.over)

        self.ui.left.pressed.connect(self.left)
        self.ui.left.released.connect(self.over)

        self.ui.right.pressed.connect(self.right)
        self.ui.right.released.connect(self.over)

        self.ui.up.pressed.connect(self.up)
        self.ui.up.released.connect(self.over)

        self.ui.down.pressed.connect(self.down)
        self.ui.down.released.connect(self.over)
        #左旋、右旋
        self.ui.left_roll.pressed.connect(self.left_whirl)
        self.ui.left_roll.released.connect(self.over)

        self.ui.right_roll.pressed.connect(self.right_whirl)
        self.ui.right_roll.released.connect(self.over)
    

    # 更新串口列表
    def update_ports(self):
        # while self.update_flag:
        # sleep(10)
        self.port_list = list(serial.tools.list_ports.comports())
        self.port_str_list=[]
        for i in range(len(self.port_list)):
            # 将串口号切割出来
            lines = str(self.port_list[i])
            str_list = lines.split(" ")
            self.port_str_list.append(str_list[0])
        self.ui.port_list.clear()
        self.ui.port_list.addItems(self.port_str_list)

        
    #打开串口
    def open_port(self):
        self.ser.port=self.ui.port_list.currentText()
        self.ser.open()
        if self.ser.isOpen():
            self.ui.open_port.setEnabled(False)
            self.ui.close_port.setEnabled(True)
            self.ui.port_list.setEnabled(False)
        else:
            self.ui.open_port.setEnabled(True)
            self.ui.close_port.setEnabled(False)
            self.ui.port_list.setEnabled(True)
    
    #关闭串口
    def close_port(self):
        self.ser.port=self.ui.port_list.currentText()
        self.ser.close()
        if self.ser.isOpen():
            self.ui.open_port.setEnabled(False)
            self.ui.close_port.setEnabled(True)
            self.ui.port_list.setEnabled(False)
        else:
            self.ui.open_port.setEnabled(True)
            self.ui.close_port.setEnabled(False)
            self.ui.port_list.setEnabled(True)


    #遥控器解锁/上锁
    def lock(self):
        '''
        LOCK      0x35
        '''
        self.ser.write(bytearray([0xaa,0xaf,0x35]))
        pass

    #飞行模式选择
    def change_mode(self):
        '''
        configParam.flight.mode=0x34;无头模式
        configParam.flight.mode=0x33;有头模式
        '''
        if(self.ui.mode.currentText()=='有头模式'):
            self.ser.write(bytearray([0xaa,0xaf,0x33]))
        else:
            self.ser.write(bytearray([0xaa,0xaf,0x34]))



    #控制方式选择
    def change_control(self):

        '''
        configParam.flight.ctrl=0x30;定高模式
        configParam.flight.ctrl=0x31;手动模式
        configParam.flight.ctrl=0x32;定点模式
        '''
        if(self.ui.control.currentText()=='定高模式'):
            self.ser.write(bytearray([0xaa,0xaf,0x30]))
        elif(self.ui.control.currentText()=='手动模式'):
            self.ser.write(bytearray([0xaa,0xaf,0x31]))
        else:
            self.ser.write(bytearray([0xaa,0xaf,0x32]))


    #一键起飞/降落，定高飞行时有效
    def fly_land(self):
        '''
        CMD_FLIGHT_LAND->0x03
        sendRmotorCmd(CMD_FLIGHT_LAND, NULL)
        '''
        fly = bytearray([0xaa,0xaf,0x50,0x03,0x00,0x03,0x00,0xaf])
        self.ser.write(fly)

        pass

    #紧急制动
    def shutdown(self):
        '''
        CMD_EMER_STOP->0x04
        sendRmotorCmd(CMD_EMER_STOP, NULL)
        '''
        shut_cmd = bytearray([0xaa,0xaf,0x50,0x03,0x00,0x04,0x00,0xB0])
        self.ser.write(shut_cmd)
            

    #增加油门，使四轴向上
    def up(self):
        self.send_flag=True
        up_cmd = bytearray([0xaa,0xaf,0x37,0x04])
        self.thread=Thread(target=self.send_cmd,args=(up_cmd,))
        self.thread.start()

    #减小油门，使四轴降低
    def down(self):
        self.send_flag=True
        down_cmd = bytearray([0xaa,0xaf,0x37,0x05])
        self.thread=Thread(target=self.send_cmd,args=(down_cmd,))
        self.thread.start()

    #左旋
    def left_whirl(self):
        self.send_flag=True
        left_whirl_cmd = bytearray([0xaa,0xaf,0x38,0x06])
        self.thread=Thread(target=self.send_cmd,args=(left_whirl_cmd,))
        self.thread.start()

    #右旋
    def right_whirl(self):
        self.send_flag=True
        right_whirl_cmd = bytearray([0xaa,0xaf,0x38,0x07])
        self.thread=Thread(target=self.send_cmd,args=(right_whirl_cmd,))
        self.thread.start()

    #向前
    def forward(self):
        self.send_flag=True
        forward_cmd = bytearray([0xaa,0xaf,0x36,0x00])
        self.thread=Thread(target=self.send_cmd,args=(forward_cmd,))
        self.thread.start()

    #向后
    def back(self):
        self.send_flag=True
        back_cmd = bytearray([0xaa,0xaf,0x36,0x01])
        self.thread=Thread(target=self.send_cmd,args=(back_cmd,))
        self.thread.start()

    #向左
    def left(self):
        self.send_flag=True
        left_cmd = bytearray([0xaa,0xaf,0x36,0x02])
        self.thread=Thread(target=self.send_cmd,args=(left_cmd,))
        self.thread.start()

    #向右
    def right(self):
        self.send_flag=True
        right_cmd = bytearray([0xaa,0xaf,0x36,0x03])
        self.thread=Thread(target=self.send_cmd,args=(right_cmd,))
        self.thread.start()
    

    #停止命令
    def over(self):
        self.send_flag=False

    
    #建立线程函数
    def send_cmd(self,cmd):
        while self.send_flag:
            self.ser.write(cmd)
        # print('子线程已结束')


app = QApplication([])
app.setWindowIcon(QIcon('logo.png'))
test = remotor()
test.ui.show()
app.exec_()
test.update_flag=False

        