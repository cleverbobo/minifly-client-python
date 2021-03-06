#include <string.h>
#include "usblink.h"
#include "hw_config.h"
#include "usb_pwr.h"
/*FreeRtos includes*/
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
//�������ӵ�ͷ�ļ�
#include "config_param.h"
#include "main_ui.h"
#include "remoter_ctrl.h"
/********************************************************************************	 
 * ������ֻ��ѧϰʹ�ã�δ��������ɣ��������������κ���;
 * ALIENTEK MiniFly_Remotor
 * USBͨ����������	
 * ����ԭ��@ALIENTEK
 * ������̳:www.openedv.com
 * ��������:2018/6/1
 * �汾��V1.0
 * ��Ȩ���У�����ؾ���
 * Copyright(C) ������������ӿƼ����޹�˾ 2014-2024
 * All rights reserved
********************************************************************************/

#define USBLINK_TX_QUEUE_SIZE 16
#define USBLINK_RX_QUEUE_SIZE 16
//��������ĺ궨�壬���ڸ���ң��ģʽ
#define ALTHOLD   0x30
#define MANUAL    0x31
#define THREEHOLD 0x32
#define X         0x33
#define HEAD      0x34
#define LOCK      0x35
#define DIRECT    0x36
#define THD       0x37
#define WHIRL     0x38

#define forward   0x00
#define back      0x01
#define left      0x02
#define right     0x03

#define up        0x04
#define down      0x05

#define left_whirl 0x06
#define right_whirl 0x07

//bool remote_start = true;
//bool over         = false;
static enum
{
	waitForStartByte1,
	waitForStartByte2,
	waitForMsgID,
	waitForDataLength,
	waitForData,
	waitForChksum1,
	//�������ӵĶ���
	change_direct,
	change_thd,
	change_whirl,
	
}rxState;

static bool isInit;
static atkp_t rxPacket;
static xQueueHandle  txQueue;
static xQueueHandle  rxQueue;


/*usb���ӳ�ʼ��*/
void usblinkInit(void)
{
	if (isInit) return;
	txQueue = xQueueCreate(USBLINK_TX_QUEUE_SIZE, sizeof(atkp_t));
	ASSERT(txQueue);
	rxQueue = xQueueCreate(USBLINK_RX_QUEUE_SIZE, sizeof(atkp_t));
	ASSERT(rxQueue);
	isInit = true;
}

/*usb���ӷ���atkpPacket*/
bool usblinkSendPacket(const atkp_t *p)
{
	ASSERT(p);
	ASSERT(p->dataLen <= ATKP_MAX_DATA_SIZE);
	return xQueueSend(txQueue, p, 0);
}
bool usblinkSendPacketBlocking(const atkp_t *p)
{
	ASSERT(p);
	ASSERT(p->dataLen <= ATKP_MAX_DATA_SIZE);
	return xQueueSend(txQueue, p, portMAX_DELAY);
}

/*usb���ӽ���atkpPacket*/
bool usblinkReceivePacket(atkp_t *p)
{
	ASSERT(p);
	return xQueueReceive(rxQueue, p, 0);
}
bool usblinkReceivePacketBlocking(atkp_t *p)
{
	ASSERT(p);
	return xQueueReceive(rxQueue, p, portMAX_DELAY);
}

/*usb���ӷ�������*/
void usblinkTxTask(void* param)
{
	atkp_t p;
	u8 sendBuffer[64];
	u8 cksum;
	u8 dataLen;
	while(bDeviceState != CONFIGURED)//��usb���óɹ�
	{
		vTaskDelay(1000);
	}
	while(1)
	{
		xQueueReceive(txQueue, &p, portMAX_DELAY);
		if(p.msgID != UP_RADIO)/*NRF51822�����ݰ����ϴ�*/
		{
			if(p.msgID == UP_PRINTF)/*��ӡ���ݰ�ȥ��֡ͷ*/
			{
				memcpy(&sendBuffer, p.data, p.dataLen);
				dataLen = p.dataLen;
			}
			else
			{
				sendBuffer[0] = UP_BYTE1;
				sendBuffer[1] = UP_BYTE2;
				sendBuffer[2] = p.msgID;
				sendBuffer[3] = p.dataLen;
				memcpy(&sendBuffer[4], p.data, p.dataLen);
				cksum = 0;
				for (int i=0; i<p.dataLen+4; i++)
				{
					cksum += sendBuffer[i];
				}
				dataLen = p.dataLen+5;
				sendBuffer[dataLen - 1] = cksum;
			}
			usbsendData(sendBuffer, dataLen);
		}		
	}
}

/*usb���ӽ�������*/
void usblinkRxTask(void *param)
{
	u8 c;
	u8 dataIndex = 0;
	u8 cksum = 0;
	rxState = waitForStartByte1;
	while(1)
	{
		if (usbGetDataWithTimout(&c))
		{
			switch(rxState)
			{
				case waitForStartByte1:
					rxState = (c == DOWN_BYTE1) ? waitForStartByte2 : waitForStartByte1;
					cksum = c;
					break;
				case waitForStartByte2:
					rxState = (c == DOWN_BYTE2) ? waitForMsgID : waitForStartByte1;
					cksum += c;
					break;
				case waitForMsgID:
					// ��ĵĴ��룬����pc�˲���ģʽ
					switch(c)
					  {
						case ALTHOLD:
							configParam.flight.ctrl = ALTHOLD_MODE;
						  break;
            case MANUAL:
							configParam.flight.ctrl = MANUAL_MODE;
						  break;
						case THREEHOLD: 		
							configParam.flight.ctrl = THREEHOLD_MODE;
					   	break;
						case HEAD:
							configParam.flight.mode = HEAD_LESS;
              break;
						case X:
							configParam.flight.mode = X_MODE;
						  break;
						case LOCK:
							setRCLock(!getRCLock());
              break;		
			      case DIRECT:
							rxState = change_direct;
						  break;
						case THD:
							rxState = change_thd;
						  break;
						case WHIRL:
							rxState = change_whirl;
						  break;							
						default:
					      rxPacket.msgID = c;
					      rxState = waitForDataLength;
					      cksum += c;
							  break;
						}
				  break;
				// �ı䷽��
				case change_direct:
					if(configParam.flight.mode==X_MODE){
							switch(c)
							{
								case forward:							
									remoterData_t send_forward={0,10.0,0,50,0,0,0x01,0,0};
									sendRmotorData((u8*)&send_forward, sizeof(send_forward));
									break;
								case back:
									remoterData_t send_back={0,-10.0,0,50,0,0,0x01,0,0};
									sendRmotorData((u8*)&send_back, sizeof(send_back));
									break;
								case left:
									remoterData_t send_left={-10.0,0,0,50,0,0,0x01,0,0};
									sendRmotorData((u8*)&send_left, sizeof(send_left));
									break;
								case right:
									remoterData_t send_right={10.0,0,0,50,0,0,0x01,0,0};
									sendRmotorData((u8*)&send_right, sizeof(send_right));
									break;
								default:
									break;
							}
						}
					 else if(configParam.flight.mode == HEAD_LESS){
						 switch(c)
							{
								case forward:							
									remoterData_t send_forward={0,10.0,0,50,0,0,0x01,1,0};
									sendRmotorData((u8*)&send_forward, sizeof(send_forward));
									break;
								case back:
									remoterData_t send_back={0,-10.0,0,50,0,0,0x01,1,0};
									sendRmotorData((u8*)&send_back, sizeof(send_back));
									break;
								case left:
									remoterData_t send_left={-10.0,0,0,50,0,0,0x01,1,0};
									sendRmotorData((u8*)&send_left, sizeof(send_left));
									break;
								case right:
									remoterData_t send_right={10.0,0,0,50,0,0,0x01,1,0};
									sendRmotorData((u8*)&send_right, sizeof(send_right));
									break;
								default:
									break;
							}
					 }
					break;
										
				case change_thd:
					if(c==up){
					   remoterData_t send_up={0,0,0,80,0,0,0x01,0,0};
             sendRmotorData((u8*)&send_up, sizeof(send_up));}
					else if(c==down){
						 remoterData_t send_down={0,0,0,20,0,0,0x01,0,0};
             sendRmotorData((u8*)&send_down, sizeof(send_down));}
					break;
				
				case change_whirl:
					if(c==left_whirl){
					   remoterData_t send_left_whirl={0,0,-100.0,50,0,0,0x01,0,0};
             sendRmotorData((u8*)&send_left_whirl, sizeof(send_left_whirl));}
					else if(c==right_whirl){
						 remoterData_t send_right_whirl={0,0,100.0,50,0,0,0x01,0,0};
             sendRmotorData((u8*)&send_right_whirl, sizeof(send_right_whirl));}
					break;
//*************************************************************************************************	
				case waitForDataLength:
					if (c <= ATKP_MAX_DATA_SIZE)
					{
						rxPacket.dataLen = c;
						dataIndex = 0;
						rxState = (c > 0) ? waitForData : waitForChksum1;	/*c=0,���ݳ���Ϊ0��У��1*/
						cksum += c;
					} else 
					{
						rxState = waitForStartByte1;
					}
					break;
				case waitForData:
					rxPacket.data[dataIndex] = c;
					dataIndex++;
					cksum += c;
					if (dataIndex == rxPacket.dataLen)
					{
						rxState = waitForChksum1;
					}
					break;
				case waitForChksum1:
					if (cksum == c)/*����У����ȷ*/
					{
						xQueueSend(rxQueue, &rxPacket, 0);
					} 
					else
					{
						rxState = waitForStartByte1;
					}
					rxState = waitForStartByte1;
					break;
				default:
					break;
			}
		}
		else	/*��ʱ����*/
		{
			rxState = waitForStartByte1;
		}
	}
}

