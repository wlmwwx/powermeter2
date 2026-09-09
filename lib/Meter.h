#ifndef	__METER_H
#define	__METER_H
#include "defin.h"

#define	Meter_int_mark 0x9698
#define	Meter_Kp  67596 //Kp=K2 10^10 / (HFConst EC 2 ^ 23)=5.625*10^10/(62*16000*2^23)=0.0067596
#define	Meter_EC  16000 //脉冲常数为160000  
#define	Meter_auto_jz_mark 0x9597

typedef enum 
{
  MeterIC_OK       = 0x00U,
  MeterIC_ERROR    = 0x01U,
} MeterIC_StatusTypeDef;

static const INT8U Meter_READ_cmd[]={0x08,0x06,0x0A,0x0B,0x0C,0x09,0x0D,0x0E};

typedef enum 
{
  Meter_U       = 0x08U,
  Meter_I1      = 0x06U,
  Meter_P1      = 0x0AU,
  Meter_Q1      = 0x0BU,
  Meter_S       = 0x0CU,
  Meter_F       = 0x09U,
  Meter_EnP     = 0x0DU,
  Meter_EnQ     = 0x0EU,
} Meter_READ_MARK;

typedef enum 
{
  MeterCaliVIPmark   = 0x00U,
  MeterCaliPFmark    = 0x01U,
} MeterCaliMarkTypeDef;

MeterIC_StatusTypeDef  Read_MeterIC_block(INT8U meter_addr,INT32U *meter_value);
MeterIC_StatusTypeDef Write_MeterIC_block(INT8U meter_addr,INT16U meter_value);
MeterIC_StatusTypeDef Init_MeterIC_Reg(INT8U ad_lu,INT16U locktmp);
MeterIC_StatusTypeDef Read_uipq(INT8U ad_lu,INT8U meterReadtmp);
INT8U RN8302_AutoCal(INT8U ad_lu,INT16U locktmp1,INT8U calimark);
void energy_add(INT8U ad_lu);

#endif






