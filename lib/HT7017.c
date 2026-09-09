#define L_HT7017
#include "para.h"
#include "gpio.h"
#include "usart.h"
#include "HT7017.h"
#include <string.h>

/*******************************************************************************
********HT7017底层驱动函数，串口通讯阻塞式
********
********
********
********
*******************************************************************************/

/*******************************************************************************
功能描述：	从HT7017内读计量参数
输入参数：	addr:		寄存器地址
			*para:		读取数据
返回参数： 返回从寄存器中读取的值，入股该值为0xf1234567，则表示读取错误
函数说明：
*******************************************************************************/
//uint8_t Read_Reg(uint8_t addr, uint32_t *para)
uint32_t Read_Reg_blocking(uint8_t addr)
{
	uint8_t	comBuf[10];														//通讯帧数据缓冲区
	uint8_t	i, sum;	
	uint8_t result;
	uint32_t para;
	
	comBuf[0] = 0x6A;
	comBuf[1] = addr & 0x7F;
	
	for (i=0; i<3; i++)
	{	
		if(HAL_OK == HAL_UART_Transmit(&huart2, comBuf, 2, 100))
		{
			result = HAL_UART_Receive(&huart2, comBuf+2, 4, 200);
			if (HAL_OK==result)								//UART接收处理
			{
				sum = comBuf[0] +comBuf[1] +comBuf[2] +comBuf[3] +comBuf[4];
				sum = ~sum;
				if (sum == comBuf[5])
				{
					para = comBuf[2]<<16 | comBuf[3]<<8 | comBuf[4];
					return para;
				}			
			}
		}
		
	}
	return 0xf1234567;
}

/*******************************************************************************
功能描述：	写校表参数
输入参数：	addr:		寄存器地址
			para:		寄存器数据
返回参数：FALSE/TRUE
函数说明：
*******************************************************************************/
uint8_t Write_Reg_blocking(uint8_t addr, uint16_t para)
{
	uint8_t	comBuf[10];										//通讯帧数据缓冲区
	uint8_t	i, sum;
	comBuf[0] = 0x6A;
	comBuf[1] = addr | 0x80;
	comBuf[2] = para>>8;
	comBuf[3] = para;
	sum	= comBuf[0] +comBuf[1] +comBuf[2] +comBuf[3];
	comBuf[4] = ~sum;
	comBuf[5] = 0x00;
	for (i=0; i<3; i++)
	{
		if(HAL_OK == HAL_UART_Transmit(&huart2, comBuf, 5, 100))
		{
			if (HAL_OK==HAL_UART_Receive(&huart2, comBuf+5, 1, 100))//UART接收处理	
			{
				if (comBuf[5] == 0x54)
				{
					return 1;
				}			
			}
		}
	}
	return 0;
}



/*******************************************************************************
功能描述：	写校表参数
输入参数：	addr:		寄存器地址
			para:		寄存器数据
返回参数：
函数说明：
*******************************************************************************/
uint8_t Uart_Write_Reg(uint8_t addr, uint16_t para)
{
	uint8_t temp=0;
	if((addr<=0x45)&&(addr>=0x40))
	{
		Write_Reg_blocking(0x32,0xBC);//关闭计量芯片写保护
	}
	 
	else if((addr<=0x7c)&&(addr>=0x50))
	{
		Write_Reg_blocking(0x32,0xA6);//关闭计量芯片写保护
	}
	temp=Write_Reg_blocking(addr,para);	 
	Write_Reg_blocking(0x32,0x00);//开启计量芯片写保护
    
	return temp;
}






