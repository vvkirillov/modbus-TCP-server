// MODBUS.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "modbus.h"

#pragma comment(lib,"Ws2_32.lib")

#define MAXSTRIN 256

unsigned _stdcall DemoInputRegistersRoutine(void* socket);
unsigned _stdcall DemoDiscreteInputsRoutine(void* socket);

struct Data{
	HANDLE  discrete_inputs_mutex;
	HANDLE  input_registers_mutex;
	char discrete_inputs; //4 
	char coils; //4
	u_short input_registers[4]; //4  
} data;

char* mdb_basicID[3];

void SetLocale()
{
	_wsetlocale(LC_ALL,L"Russian");
}

void DebugGetDiscreteInputs(char* regbuffer,u_short start,u_short quantity)
{
	//WaitForSingleObject(data.discrete_inputs_mutex,INFINITE);
	*regbuffer = data.discrete_inputs >> start;
	*regbuffer &= (0xFF >> (8 - quantity));
	_ReadWriteBarrier();
	//ReleaseMutex(data.discrete_inputs_mutex);
}

void DebugGetInputRegisters(char* regbuffer,u_short start,u_short quantity)
{
	/////
	//WaitForSingleObject(data.input_registers_mutex,INFINITE);
	wmemcpy_s((wchar_t*)regbuffer,quantity,
		(wchar_t*)&data.input_registers[start],quantity);
	//ReleaseMutex(data.input_registers_mutex);
	//Форсирование операции записи для многопотoчного использования
	_ReadWriteBarrier();
	/////
}
void DebugWriteSingleCoil(u_short addr,u_short value)
{
	if(value == 0xFF)
		data.coils |= (1 << addr);
	else
		data.coils &= ~(1 << addr);
}

int DebugGetDeviceID(char*buffer, char readDevID, char objectID)
{
	//поддерживаются только базовая идентификация (чтение всех параметров)
	//и специфическая
	int index = 5;
	int len;
	int obj;

	//формирование ответа

	//уровень подтверждения - базовая идентификация
	buffer[0] = 1;
	//ответ умещается в рамки PDU, дополнительных транзакций не требуется
	buffer[1] = 0;
	//дополнительных транзакций не требуется, поэтому = 0 (параметр не используется)
	buffer[2] = 0;

	//ReadDevice ID = 1
	//передаем информацию обо всех объектах
	if(readDevID == 1)
	{	
		//число объектов
		buffer[4] = 3;

		//объекты
		for(obj = 0;obj<3;obj++)
		{
			//id 
			buffer[index] = obj;
			index++;
			//длина
			len = buffer[index] = strlen(mdb_basicID[obj]);
			index++;
			//значение
			memcpy_s(&buffer[index],len,mdb_basicID[obj],len);
			index += len;
		}
		
	}
	else {
		//специфический доступ
		//число объектов
		buffer[4] = 1;
		
		//id 
		buffer[index] = objectID;
		index++;
		//длина
		len = buffer[index] = strlen(mdb_basicID[objectID]);
		index++;
		//значение
		memcpy_s(&buffer[index],len,mdb_basicID[objectID],len);
		index += len;
	}

	return index;
}

int wmain(int argc, wchar_t* argv[], wchar_t *envp[ ])
{
	extern struct Callbacks_struct callbacks;
	unsigned int threadID;
	uintptr_t hInpRegThread;
	uintptr_t hDiscInpThread;
	wchar_t strin[MAXSTRIN];

	SetLocale();

	//инициализация генератора случайных чисел
	srand( (unsigned)time( NULL ) );
	
	data.input_registers_mutex = CreateMutex (NULL, FALSE, NULL);

	//инициализация функций обратного вызова
	callbacks.getdiscreteinputs_callback = DebugGetDiscreteInputs;
	callbacks.getinputregisters_callback = DebugGetInputRegisters;
	callbacks.writesinglecoil_callback = DebugWriteSingleCoil;
	callbacks.getdeviceid_callback = DebugGetDeviceID;

	//Идентификация
	mdb_basicID[0] = "Demo vendor";
	mdb_basicID[1] = "Simple modbus TCP/IP server";
	mdb_basicID[2] = "0.1";

	ModbusInit(INADDR_LOOPBACK,502);

	if(hInpRegThread = _beginthreadex( NULL, 0,&DemoInputRegistersRoutine,
			NULL,0 ,&threadID)==-1L)
	{
		WriteLog(L"Невозможно запустить клиентский поток");
		return 1;
	}

	if(hDiscInpThread = _beginthreadex( NULL, 0,&DemoDiscreteInputsRoutine,
			NULL,0 ,&threadID)==-1L)
	{
		WriteLog(L"Невозможно запустить клиентский поток");
		return 1;
	}

	do{
		_wcslwr_s(_getws_s(strin,MAXSTRIN),MAXSTRIN);
	}while(wcscmp(strin,L"exit")!=0 && wcscmp(strin,L"e") != 0);

	WriteLog(L"Завершение приложения");
	
	ReleaseMutex(data.input_registers_mutex);
	ReleaseMutex(data.discrete_inputs_mutex);

	CloseHandle((HANDLE)hInpRegThread);
	CloseHandle((HANDLE)hDiscInpThread);

	WSACleanup();
	return 0;
}

unsigned _stdcall DemoInputRegistersRoutine(void* socket)
{
	int i = 0;
	int sign = 1;
	u_short reg = 0;
	
	//статические данные
	data.input_registers[0] = 0x5fff;
	data.input_registers[1] = 0x7fff;
	data.input_registers[2] = 0x2ffE;
	data.input_registers[3] = 0x3fff;
	while(true){
		//изменение значений входных регистров
		//номер регистра
		i = getrandom(MODBUS_MAX_INPUT_REGISTERS);
		//знак
		sign = 0 - sign;
		//новое значение
		reg = getrandom(1500)*sign;

		//потокобезопасная операция сложения
		_InterlockedExchangeAdd((long*)&data.input_registers[i],reg);

		Sleep(1000);
	}
	return 0;
}
unsigned _stdcall DemoDiscreteInputsRoutine(void* socket)
{
	int i = 0;
	int set = 1;
	while(true){
		//номер регистра
		i = getrandom(MODBUS_MAX_DISCRETE_INPUTS);
		//установить или сбросить
		set = getrandom(2);

		//WaitForSingleObject(data.discrete_inputs_mutex,INFINITE);
		if(set)
			//data.discrete_inputs |= (1 << i);
			_InterlockedOr((long*)&data.discrete_inputs, (1 << i));
		else
			//data.discrete_inputs &= ~(1 << i);
			_InterlockedAnd((long*)&data.discrete_inputs, ~(1 << i));
		//ReleaseMutex(data.discrete_inputs_mutex);

		Sleep(1000);
	}
	return 0;
}

