#pragma once
#include "stdafx.h"



#define MODBUS_MAX_DISCRETE_INPUTS 4
#define MODBUS_MAX_INPUT_REGISTERS 4

#define MODBUS_MAX_COILS 4

//длина MPAB-заголовка
#define MODBUS_MPAB_LEN 7
//максимальная длина фрейма данных
#define MODBUS_MAX_PDU 253
//максимальная длина полного сообщения (MPAB+PDU)
#define MODBUS_MAX_ADU 260

#define MODBUS_MAX_DENIED_IP 260

//максимальный тайм-аут чтения / записи
#define MODBUS_TRANS_TIMEOUT 3000

#define IPBUFSIZE 256

#define getrandom(val) (rand()%(int)(val))

#define GET_PDU_LEN(mbap) GetTCPShort(&(mbap)[4])

#define BYTES_CEILING(x) ((x) >> 3) + ( ((x) & 7) != 0 ? 1 : 0)


#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#ifndef bool
#define bool boolean
#endif