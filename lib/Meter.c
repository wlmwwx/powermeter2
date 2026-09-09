#define L_Meter
#include "para.h"
#include "gpio.h"
#include "usart.h"
#include "HT7017.h"
#include <string.h>
#include <stdlib.h>
#include "Meter.h"
#include "WatchDog.h"
/*******************************************************************************
********HT7017底层驱动函数，串口通讯阻塞式
********
********
********
********
*******************************************************************************/

/*************************************************************************************
**  函数名称：Init_MeterIC_Reg()
**  功能描述：初始化计量芯片         
**  入口参数：INT8U ad_lu:计量芯片的编号，只有一个计量芯片时此值为0；
              INT16U locktmp:初始化锁，防止程序跑飞误入该函数；
**  出口参数：MeterIC_ERROR，失败，MeterIC_OK，成功
**  作    者：   2021-03-15
*************************************************************************************/
MeterIC_StatusTypeDef Init_MeterIC_Reg(INT8U ad_lu,INT16U locktmp)
{

	INT8U i,j,numbCali;
	INT32U mid32u,mid32u1;
	if((locktmp!=Meter_int_mark)&&(ad_lu>=METERnum))
		return MeterIC_ERROR;
	
	mid32u  = sizeof(EC_RegData);
	mid32u1 = sizeof(EC_RegData[0]);
	mid32u /= mid32u1;//得到数组里有多少个结构体元素
	j=ad_lu;
	numbCali = mid32u;

	MeterCaliPara[j].CheckSum = 0;
	for (i=0; i<numbCali; i++)
	{
		if(strstr(EC_RegData[i].name,"EMUCFG"))//初始化EMUCFG，电能为读后清零型，有功、无功为绝对值和
		{
			mid32u = 0x3400;	
		}
		else if(strstr(EC_RegData[i].name,"GP1"))//初始化功率增益
		{
			mid32u = MeterCaliPara[j].P1gain;
		}
		else if(strstr(EC_RegData[i].name,"I1RMSOff"))//初始化电流零点
		{
			mid32u = MeterCaliPara[j].I1offset;
		}		
		else if(strstr(EC_RegData[i].name,"GPhs1"))//初始化移相角
		{
			mid32u = MeterCaliPara[j].UI1COS;		
		}
		else
		{
			mid32u = EC_RegData[i].rst;	//非这个参数初始化入默认值
		}
		if(Write_MeterIC_block(EC_RegData[i].addr,mid32u)==MeterIC_ERROR)
			return MeterIC_ERROR;
		if(((EC_RegData[i].addr>=0x40)&&(EC_RegData[i].addr<0x6f))||((EC_RegData[i].addr>0x74)&&(EC_RegData[i].addr<=0x7c)))//校验和区间0X40~0X7C，不包括0X6F~0X74
			MeterCaliPara[j].CheckSum += mid32u;	//计算24位校验和，用于实时判断校准参数是否错乱
	}
	return MeterIC_OK;
}


/*************************************************************************************
**  函数名称：Read_MeterIC_block()
**  功能描述：读取计量芯片内的寄存器值         
**  入口参数：INT8U meter_addr:计量芯片寄存器的地址号；
             INT32U *meter_value:读取的值放置的指针；
**  出口参数：读取成功否？MeterIC_ERROR，失败，MeterIC_OK，成功
**  作    者：   2021-03-15
*************************************************************************************/
MeterIC_StatusTypeDef Read_MeterIC_block(INT8U meter_addr,INT32U *meter_value)
{
	*meter_value = Read_Reg_blocking(meter_addr);
	if(*meter_value == 0xf1234567)//特殊标记字符，读上来是这个值则表示读取错误
	{
		*meter_value = 0;
		return MeterIC_ERROR;
	}
	else
	{
		return MeterIC_OK;
	}
}

/*************************************************************************************
**  函数名称：Write_MeterIC_block()
**  功能描述：写入计量芯片内的寄存器值         
**  入口参数：INT8U meter_addr:计量芯片寄存器的地址号；
             INT32U meter_value:写入计量芯片的值；
**  出口参数：读取成功否？MeterIC_ERROR，失败，MeterIC_OK，成功
**  作    者：   2021-03-15
*************************************************************************************/
MeterIC_StatusTypeDef Write_MeterIC_block(INT8U meter_addr,INT16U meter_value)
{
	if (Uart_Write_Reg(meter_addr, meter_value) == 1)
	{
		return MeterIC_OK;
	}
	else
	{
		return MeterIC_ERROR;
	
	}
}

/*************************************************************************************
**  函数名称：Read_uipq()
**  功能描述：从计量芯片内读取实时电参数         
**  入口参数：INT8U ad_lu:读取的计量芯片号，默认0；
             INT8U meterReadtmp：读取的参数编号，详见Meter_READ_MARK定义；
**  出口参数：读取成功否？MeterIC_ERROR，失败，MeterIC_OK，成功
**  作    者：   2021-03-16
*************************************************************************************/
MeterIC_StatusTypeDef Read_uipq(INT8U ad_lu,INT8U meterReadtmp)
{
	INT8U readmark;
	INT64 mid64s;
	INT32S mid32s;
	INT32U mid32u,mid32u1;
	ad_lu = ad_lu;
	if(meterReadtmp>=sizeof(Meter_READ_cmd))//读取编号不超过实际编号
		return MeterIC_ERROR;
	
	readmark = Meter_READ_cmd[meterReadtmp];
	switch(readmark)
	{
		case Meter_U:							//读取电压值
			Read_MeterIC_block(Meter_U,&mid32u);
//		    MeterCaliPara[0].Ugain=10360;
			mid64s = mid32u;    
		    mid64s = mid64s * MeterCaliPara[0].Ugain/10000000;//乘以电压增益，并转换为0.1V单位
			mid32s = mid64s;
			G_RealTmData[0].Uabc_single[0][0] = mid32s;		  //放入寄存器
			G_DevSts.voltage = G_RealTmData[0].Uabc_single[0][0];
			snprintf((INT8 *)Uart6_para.uart_send_buff,30,"U:%d,Urms:%d\r",mid32u,mid32s);
			HAL_UART_Transmit_IT(&huart6, Uart6_para.uart_send_buff, strlen((INT8 *)Uart6_para.uart_send_buff));		
		break;
		case Meter_I1:						   //读取电流值
			Read_MeterIC_block(Meter_I1,&mid32u);
//			MeterCaliPara[0].I1gain = 14343;
			mid64s = mid32u;
		    mid64s = mid64s * MeterCaliPara[0].I1gain/1000000;//乘以电流增益，并转换为1mA单位
			mid32s = mid64s;
			G_RealTmData[0].Iabc_single[0][0] = mid32s;
			G_DevSts.current = G_RealTmData[0].Iabc_single[0][0];
			snprintf((INT8 *)Uart6_para.uart_send_buff,30,"I1:%d,I1rms:%d\r",mid32u,mid32s);
			HAL_UART_Transmit_IT(&huart6, Uart6_para.uart_send_buff, strlen((INT8 *)Uart6_para.uart_send_buff));		
		break;
		case Meter_P1:						   //读取有功功率值
			Read_MeterIC_block(Meter_P1,&mid32u);
			if(mid32u > 0x800000)
				mid32u |= 0xff000000;
			mid32s = mid32u;
			mid32u = abs(mid32s);
			mid64s = mid32u ;
			mid64s = mid64s*Meter_Kp/10000000;//乘以转换系数，并转换为1W单位
			mid32u = mid64s;
			G_RealTmData[0].Pabc_single[0][0] = mid32u;
			G_DevSts.power = mid32u;
			snprintf((INT8 *)Uart6_para.uart_send_buff,30,"P1:%d\r",mid32u);
			HAL_UART_Transmit_IT(&huart6, Uart6_para.uart_send_buff, strlen((INT8 *)Uart6_para.uart_send_buff));		
		break;
		case Meter_Q1:						   //读取无功功率值
			Read_MeterIC_block(Meter_Q1,&mid32u);
			if(mid32u > 0x800000)
				mid32u |= 0xff000000;
			mid32s = mid32u;
			mid32u = abs(mid32s);
			mid64s = mid32u ;
			mid64s = mid64s*Meter_Kp/10000000;
			mid32u = mid64s;
			G_RealTmData[0].Qabc_single[0][0] = mid32u;
			snprintf((INT8 *)Uart6_para.uart_send_buff,30,"Q1:%d\r",mid32u);
			HAL_UART_Transmit_IT(&huart6, Uart6_para.uart_send_buff, strlen((INT8 *)Uart6_para.uart_send_buff));		
		break;	
		case Meter_S:						   //读取视在功率值
			Read_MeterIC_block(Meter_S,&mid32u);
			if(mid32u > 0x800000)
				mid32u |= 0xff000000;
			mid32s = mid32u;
			mid32u = abs(mid32s);
			mid64s = mid32u ;
			mid64s = mid64s*Meter_Kp/10000000;
			mid32u = mid64s;
			G_RealTmData[0].Sabc_single[0][0] = mid32u;
			G_RealTmData[0].PFabc_single[0][0] = G_RealTmData[0].Pabc_single[0][0]*1000/G_RealTmData[0].Sabc_single[0][0];
			G_DevSts.factor = G_RealTmData[0].PFabc_single[0][0];
			snprintf((INT8 *)Uart6_para.uart_send_buff,30,"S:%d,PF:%d\r",mid32u,G_RealTmData[0].PFabc_single[0][0]);
			HAL_UART_Transmit_IT(&huart6, Uart6_para.uart_send_buff, strlen((INT8 *)Uart6_para.uart_send_buff));		
		break;
		case Meter_F:						           //读取频率值
			Read_MeterIC_block(Meter_F,&mid32u);
			mid32s = 5000000;
			mid32u1 = mid32s/mid32u;
			G_RealTmData[0].Fre = mid32u1;            //频率
			snprintf((INT8 *)Uart6_para.uart_send_buff,30,"Frep:%d\r",G_RealTmData[0].Fre);
			HAL_UART_Transmit_IT(&huart6, Uart6_para.uart_send_buff, strlen((INT8 *)Uart6_para.uart_send_buff));
		break;
		case Meter_EnP:						          //有功电能值
			mid32u = 0;
			if(MeterIC_OK == Read_MeterIC_block(Meter_EnP,&mid32u))
			{
				if(mid32u<50)
					MeterCaliPara[0].EP_CNT += mid32u;	//加入电能缓存		
				snprintf((INT8 *)Uart6_para.uart_send_buff,30,"EP:%d\r",mid32u);
				HAL_UART_Transmit_IT(&huart6, Uart6_para.uart_send_buff, strlen((INT8 *)Uart6_para.uart_send_buff));				
			}
			
		
		break;
		case Meter_EnQ:						          //无功电能值
			mid32u = 0;
			if(MeterIC_OK == Read_MeterIC_block(Meter_EnQ,&mid32u))
			{
				if(mid32u<50)
					MeterCaliPara[0].EQ_CNT += mid32u;//加入电能缓存	
				snprintf((INT8 *)Uart6_para.uart_send_buff,30,"EQ:%d\r",mid32u);
				HAL_UART_Transmit_IT(&huart6, Uart6_para.uart_send_buff, strlen((INT8 *)Uart6_para.uart_send_buff));
			}
		break;	
		default:
			return MeterIC_ERROR;
	}
	return MeterIC_OK;

}

/*************************************************************************************
**  函数名称：energy_add()
**  功能描述：电能累加函数         
**  入口参数：INT8U ad_lu:累加的芯片号，默认0；
          
**  出口参数：无
**  作    者：   2021-03-16
*************************************************************************************/
void energy_add(INT8U ad_lu)
{

	INT16U yg_dl_add,wg_dl_add,EC_tmp;
	if(ad_lu>=METERnum)
		return;
	
	if((MeterCaliPara[0].EP_CNT == 0)&&(MeterCaliPara[0].EQ_CNT == 0))//如果没有电能脉冲产生则直接退出
		return;
	
	EC_tmp = Meter_EC/100;//计算没0.01度电的脉冲数
	yg_dl_add = MeterCaliPara[0].EP_CNT/EC_tmp;//超过0.01度电计入
	wg_dl_add = MeterCaliPara[0].EQ_CNT/EC_tmp;
	MeterCaliPara[0].EP_CNT %= EC_tmp;         //未超过0.01度电脉冲数取余继续累加
	MeterCaliPara[0].EQ_CNT %= EC_tmp;
	
	if(yg_dl_add>10)//不能超过范围
		yg_dl_add = 0;
	if(wg_dl_add>10)
		wg_dl_add = 0;
	
	/*
	j = 0;
	if(Eng_para_dt_bak.Comb_active_energy[ad_lu][0]==G_RealTmData[0].Comb_active_energy[ad_lu][0])
	{
		j++;
	}
	if(Eng_para_dt_bak.Comb_reactive_energy[ad_lu][0]==G_RealTmData[0].Comb_reactive_energy[ad_lu][0])
	{
		j++;
	}		
	if(j!=2)//电能与备份电能有不一致的地方，重读铁电
	{
		FM_energy_read(mm);
		ErrCode_record(mm+141);	//错误代码记录
	}	*/
	G_RealTmData[0].Comb_active_energy[ad_lu][0]   += yg_dl_add;//累加入总的有无功电能寄存器中
	G_RealTmData[0].Comb_reactive_energy[ad_lu][0] += wg_dl_add;	
	
	//备份电能赋值
	/*
	Eng_para_dt_bak.Comb_active_energy[ad_lu][0]   = G_RealTmData[0].Comb_active_energy[ad_lu][0];
	Eng_para_dt_bak.Comb_reactive_energy[ad_lu][0] = G_RealTmData[0].Comb_reactive_energy[ad_lu][0];

	FM_energy_SAVE(mm);	//电量存储函数
	*/	
	snprintf((INT8 *)Uart6_para.uart_send_buff,80,"Comb_active_energy:%d\r",G_RealTmData[0].Comb_active_energy[ad_lu][0]);
	HAL_UART_Transmit_IT(&huart6, Uart6_para.uart_send_buff, strlen((INT8 *)Uart6_para.uart_send_buff));	
//	snprintf((INT8 *)Uart6_para.uart_send_buff,80,"Comb_reactive_energy:%d\r",G_RealTmData[0].Comb_reactive_energy[ad_lu][0]);
//	HAL_UART_Transmit_IT(&huart6, Uart6_para.uart_send_buff, strlen((INT8 *)Uart6_para.uart_send_buff));
}


/*************************************************************************************
**  函数名称：RN8302_AutoCal()
**  功能描述：rn8302自动校准函数         
**  入口参数：INT8U ad_lu:校准的芯片号，默认0；
             INT16U locktmp1：锁，防止函数误入
             INT8U calimark：校准的选项，参见MeterCaliMarkTypeDef
**  出口参数：0失败，1成功
**  作    者：  2021-03-16
*************************************************************************************/
INT8U RN8302_AutoCal(INT8U ad_lu,INT16U locktmp1,INT8U calimark)
{
	INT8U i=0;

	INT32S temp[4][10];		
	INT32S FReg;
	INT32S U,I,P;	
	INT64 mid64s;
	INT32S mid32s;
	INT32U mid32u;	

	if(locktmp1!=Meter_auto_jz_mark)
		return 0;

	if(calimark == MeterCaliVIPmark)//电压、电流、功率增益校准
	{
		MeterCaliPara[0].P1gain = 0;//功率增益清零
	}
	else if(calimark == MeterCaliPFmark)//功率因数校准
	{
		MeterCaliPara[0].UI1COS = 0;//功率因数校准参数清零
	}
	else
		return 0;
	
	Init_MeterIC_Reg(0,Meter_int_mark);//计量芯片初始化
	
	HAL_Delay (300);	//延时2000mS等待稳定
	WDG_Feed();//----添加喂狗-
	HAL_Delay (300);	//延时2000mS等待稳定
	WDG_Feed();//----添加喂狗-
	HAL_Delay (300);	//延时2000mS等待稳定
	WDG_Feed();//----添加喂狗-
	HAL_Delay (300);	//延时2000mS等待稳定
	WDG_Feed();//----添加喂狗-
	HAL_Delay (300);	//延时2000mS等待稳定
	WDG_Feed();//----添加喂狗-
	HAL_Delay (300);	//延时2000mS等待稳定
	WDG_Feed();//----添加喂狗-
  //读取所有相关电参数，多次采集，数据滤波、处理、平均
	for(i=0;i<10;i++)
	{
		Read_MeterIC_block(Meter_U,&mid32u);
		temp[0][i] = mid32u;
		Read_MeterIC_block(Meter_I1,&mid32u);
		temp[1][i] = mid32u;
		Read_MeterIC_block(Meter_P1,&mid32u);
		if(mid32u > 0x800000)
			mid32u |= 0xff000000;
		mid32s = mid32u;
		mid32u = abs(mid32s);
		mid64s = mid32u ;
		mid64s = mid64s*Meter_Kp/10000000;
		mid32u = mid64s;
		temp[2][i] = mid32u;

		HAL_Delay (300);	//延时300mS等待稳定
		WDG_Feed();//----添加喂狗-
	}	 
	//MeterCaliPFmark
	if(calimark == MeterCaliVIPmark)//功率因数校准,U:220V,I:5A,φ：0°时校准
	{
		U=DataAver(temp[0], 10);
		I=DataAver(temp[1], 10);
		P=DataAver(temp[2], 10);
				
		if((U>2300000)||(U<1900000))
			return 0;
		if((I>800000)||(I<620000))
			return 0;		
		if((P>1400)||(P<950))
			return 0;		
		
		mid64s = 22000000000;
		mid64s /= U;
		MeterCaliPara[0].Ugain = mid64s;
		

		mid64s = 5000000000;
		mid64s /= I;
		MeterCaliPara[0].I1gain = mid64s;		
		
		FReg=(INT32S)(1100-P);
		FReg=FReg*32768;
		FReg/=P;
		MeterCaliPara[0].P1gain = FReg&0xFFFF;
	}
	else if(calimark == MeterCaliPFmark)//功率因数校准,U:220V,I:5A,φ：60°时校准
	{
		P=DataAver(temp[2], 10);
		if((P>600)||(P<500))
			return 0;		
		FReg=(INT32S)(550-P);
		FReg=FReg*32768;
		FReg/=P;
		FReg*=1000;
		FReg/=1732;
		MeterCaliPara[0].UI1COS = FReg&0xFFFF;	
	}
		
	Init_MeterIC_Reg(0,Meter_int_mark);//计量芯片初始化
//	FM_cali_SAVE(j);		  //校准参数存储函数
	return(0x01);//返回校准成功代码
} 
