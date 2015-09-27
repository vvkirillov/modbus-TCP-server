#pragma once

#include "stdafx.h"
#include "Log.h"
#include "debug.h"

__inline u_short GetTCPShort(char* buffer){ return ntohs(*((u_short*)buffer)) ;}

__inline void MakeTCPBytes(char* buffer,u_short num) {
	*((u_short*)buffer) = htons(num)	;}


enum MODBUS_FUNCTIONS {	MODBUS_READ_COIL_STATUS = 0x1,
						MODBUS_READ_DISCRETE_INPUTS,
						MODBUS_READ_HODING_REGISTERS,
						MODBUS_READ_INPUT_REGISTERS,
						MODBUS_WRITE_SINGLE_COIL = 0x5,
						MODBUS_PRESET_SIGLE_REGISTER,
						MODBUS_WRITE_MULTIPLE_COILS = 0x0F,
						MODBUS_PRESET_MULTIPLE_REGISTERS = 0x10,
						MODBUS_READ_DEVICE_IDENTIFICATION = 0x2B
};

enum MODBUS_ERRORS {
	MODBUS_ILLEGAL_FUNCTION = 0x1,
	MODBUS_ILLEGAL_DATA_ADDRESS,
	MODBUS_ILLEGAL_DATA_VALUE
};

typedef void (*GetStoreBytesCallback)(char*,u_short,u_short);
typedef void (*SetStoreBytesCallback)(u_short,u_short);
typedef int (*GetDeviceIDCallback)(char*,char,char);


struct Callbacks_struct{
	GetStoreBytesCallback getcoils_callback;
	GetStoreBytesCallback getdiscreteinputs_callback;
	GetStoreBytesCallback getinputregisters_callback;
	SetStoreBytesCallback writesinglecoil_callback;
	GetDeviceIDCallback	getdeviceid_callback;
};


//using namespace std;
unsigned _stdcall ServerRoutine(void* );

unsigned _stdcall ClientRoutine(void* );

int ModbusInit(u_int localaddr, u_int port);

char ModbusReadDiscreteInputsFunc(char* mbap, char* frame);
char ModbusReadInputRegisters(char* mbap, char* frame);
char ModbusWriteSingleCoilFunc(char* mbap, char* frame);
char ModbusReadDeviceIdentificationFunc(char* mbap, char* frame);
char ModbusExceptionRsp(char* mbap, char* frame,enum MODBUS_ERRORS err);


int recvall( SOCKET s, char* buf,int len,int flags);
int sendall(SOCKET s, char *buf, int len, int flags);

