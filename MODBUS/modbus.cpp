#include "modbus.h"

struct Callbacks_struct callbacks;

int deniedIP[MODBUS_MAX_DENIED_IP];

int recvall_ov(WSAOVERLAPPED* clientOverlapped,HANDLE ev,SOCKET s, char* buf,
				 int len,DWORD timeout,DWORD flags)
{
	DWORD res = 0, templen = len,tempflags = flags;
	int i = 0;
	WSABUF dataBuf;
	int waitRes;
	
	ZeroMemory(clientOverlapped,
		sizeof(WSAOVERLAPPED));

	clientOverlapped->hEvent = ev;
	WSAResetEvent(clientOverlapped->hEvent);
	
	
	do {
		dataBuf.len = templen;
		dataBuf.buf = &buf[i];
		flags = tempflags;

        if(WSARecv(s,&dataBuf,
				1,&res,&flags,clientOverlapped,NULL)==SOCKET_ERROR)
		{
			int x = WSAGetLastError();
			if(x != WSA_IO_PENDING)
			{
				return 0;
			}
		}
		waitRes = WSAWaitForMultipleEvents(1,&clientOverlapped->hEvent,false,
			timeout,FALSE);
		if(waitRes == WSA_WAIT_TIMEOUT)
		{
			return 0;
		}
		WSAResetEvent(clientOverlapped->hEvent);

		WSAGetOverlappedResult(s,clientOverlapped,
			&res,false,&flags);

        if ( res > 0 )
		{
			templen -= res;
			i += res;
		}
		else 
			return res;
    } while( templen > 0 );
	return len;
}

int sendall_ov(WSAOVERLAPPED* clientOverlapped,HANDLE ev, SOCKET s, char* buf,
				 int len,DWORD timeout,DWORD flags)
{
	DWORD res = 0, templen = len,tempflags = flags;
	int i = 0;
	WSABUF dataBuf;
	int waitRes;
	
	ZeroMemory(clientOverlapped,
		sizeof(WSAOVERLAPPED));

	clientOverlapped->hEvent = ev;
	WSAResetEvent(clientOverlapped->hEvent);
	
	
	do {
		dataBuf.len = templen;
		dataBuf.buf = &buf[i];
		flags = tempflags;

        if(WSASend(s,&dataBuf,
				1,&res,flags,clientOverlapped,NULL)==SOCKET_ERROR)
		{
			int x = WSAGetLastError();
			if(x != WSA_IO_PENDING)
			{
				return 0;
			}
		}
		waitRes = WSAWaitForMultipleEvents(1,&clientOverlapped->hEvent,false,
			timeout,FALSE);
		if(waitRes == WSA_WAIT_TIMEOUT)
		{
			return 0;
		}
		WSAResetEvent(clientOverlapped->hEvent);

		WSAGetOverlappedResult(s,clientOverlapped,
			&res,false,&flags);

        if ( res > 0 )
		{
			templen -= res;
			i += res;
		}
		else 
			return res;
    } while( templen > 0 );
	return len;
}



bool ModbusReadDeniedIP()
{
	FILE* f;
	char ipbuf[IPBUFSIZE];
	int i = 0;

	if(_wfopen_s(&f,L"denied.inf",L"r")!= 0)
		return false;
	
	while((fgets(ipbuf,IPBUFSIZE,f)!=NULL) && (i < MODBUS_MAX_DENIED_IP))
	{
		deniedIP[i] = inet_addr(ipbuf);
		i++;
	}
	fclose(f);
	return true;
}

int ModbusInit(u_int localaddr, u_int port)
{
	WORD wVersionRequested;
	WSADATA wsaData;
	SOCKET sockServer;
	struct sockaddr_in saServer;
	unsigned threadID;
	uintptr_t threadHandle;
	int err;

	if(!ModbusReadDeniedIP())
	{
		WriteLog(L"Невозможно открыть файл denied.inf");
		return -1;
	}

	wVersionRequested = MAKEWORD( 2, 2 );

	err = WSAStartup(wVersionRequested,&wsaData);
	if (err != 0)
	{
		WriteLogWSAError(L"Невозможно инициализировать библиотеку WinSock");
		return -1;
	}
	WriteLog(L"Инициализация библиотеки WinSock выполнена");
	
	saServer.sin_addr.s_addr = htonl(localaddr);
	saServer.sin_family = AF_INET;														
	saServer.sin_port = htons(port);

	sockServer = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(sockServer == SOCKET_ERROR)
	{
		WriteLogWSAError(L"Ошибка инициализации сокета");
		return -1;
	}
	
	WriteLog(L"Сокет сервера создан, Socket ID = %d",sockServer);

	if(bind(sockServer,(struct sockaddr*)&saServer,
		sizeof(saServer)) == SOCKET_ERROR)
	{
		closesocket(sockServer);
		WriteLogWSAError(L"Ошибка привязки сокета");
		return -1;
	}
	WriteLog(L"Сокет сервера привязан");

	if(listen(sockServer,SOMAXCONN)==SOCKET_ERROR)
	{
		closesocket(sockServer);
		WriteLogWSAError(L"Ошибка прослушивания");
		return -1;
	}
	WriteLog(L"Прослушивание порта запущено");

	//Т.к. в C адреса указателей передаются по значению, передаем номер
	//сокета сервера как "адрес" в серверный поток
	if(threadHandle = _beginthreadex( NULL, 0,&ServerRoutine,
			(void*)sockServer,0 ,&threadID)==-1L)
	{
		closesocket(sockServer);
		WriteLog(L"Невозможно запустить серверный поток");
		return -1;
	}
	return 0;
}

unsigned _stdcall ServerRoutine(void* socket)
{
	SOCKET clientSock;
	
	uintptr_t clientThread; 
	uintptr_t threadID;
	uintptr_t curThreadID = GetCurrentThreadId();
	bool res = false;
	int i = 0;
	int ret = 0;

	struct sockaddr localaddr;
	int size = sizeof(struct sockaddr);

	//сокет сервера, значение передано как адрес
	SOCKET serverSock = (SOCKET) socket;

	WriteLog(L"Поток сервера запущен, ThreadID = %d",curThreadID);

	while(true)
	{
		if((clientSock = accept(serverSock,
			(struct sockaddr*)&localaddr,&size))
			== INVALID_SOCKET)
		{
			WriteLogWSAError(L"Сокет недействителен");
			ret = 1;
			break;
		}

		//проверка валидности ip-адреса
		for (i = 0;i < MODBUS_MAX_DENIED_IP;i++)
		{
			if(((struct sockaddr_in*)&localaddr)->sin_addr.S_un.S_addr == deniedIP[i])
			{
				res = true;
				break;
			}
		}

		if(res)
		{
			WriteLog(L"IP-адрес клиента находится в списке запрещенных");
			closesocket(clientSock);
			continue;
		}

		WriteLog(L"Соединение установлено, Socket ID = %d",clientSock);

		//Т.к. в C адреса указателей передаются по значению, передаем номер
		//сокета клиента как "адрес" в клиентский поток
		if(clientThread = _beginthreadex( NULL, 0,&ClientRoutine,
		 	(void*)clientSock,0 ,&threadID)==-1L)
		{
			WriteLog(L"Невозможно запустить клиентский поток для сокета %d",
				clientSock);
			ret = 1;
			break;
		}
	}
	WriteLog(L"Сокет сервера N = %d закрыт",serverSock);

	closesocket(serverSock);

	WriteLog(L"Завершение серверного потока, ThreadID = %d",curThreadID);
	_endthreadex(ret);
	return ret;
}

unsigned _stdcall ClientRoutine(void* socket)
{
	SOCKET clientSock = (SOCKET) socket;

	int ret = 0;
	DWORD recvBytes = 0, flags = 0;
	HANDLE wsaEvent = WSACreateEvent();

	char mbap_buffer[MODBUS_MPAB_LEN];
	char frame_buffer[MODBUS_MAX_PDU];
	char receive_buffer[MODBUS_MAX_ADU];

	WSAOVERLAPPED clientOverlapped;
	const int MAXFRAME_LEN = 1024;
	uintptr_t curThreadID = GetCurrentThreadId();
	char num = 0;
	int datasize;

	WriteLog(L"Поток клиента запущен, ThreadID = %d",curThreadID);

	while(true)
	{

		//чтение MBAP
		int waitRes = recvall_ov(&clientOverlapped,wsaEvent,
			clientSock,mbap_buffer,
			MODBUS_MPAB_LEN,MODBUS_TRANS_TIMEOUT,0);
		if(waitRes == 0)
		{
			WriteLog(L"Ошибка чтения MBAP или истек тайм-аут ожидания MBAP");
			ret = 1;
			break;
		}

		//байты сообщения
		//---------------------------------------------------------------
		//0			1		2		3		4		5			6		
		// ID транзакции | ID-протокола | Длина сообщения | ID удаленного
		//				 |				|				  | подчиненного
		//---------------|--------------|-----------------|--------------
		//копируется	 |		0		|инициализируется | копируется 
		//из клинетского |				|клиентом и		  |из клиентского
		//запроса		 |				|сервером		  |запроса
		//---------------------------------------------------------------

		//размер фрейма данных, преобазуем из Big_Endian формата в
		//Little_endian для процессоров Intel
		datasize = GetTCPShort(&mbap_buffer[4]) - 1;

		if(mbap_buffer[2] != 0 || mbap_buffer[3] != 0)
		{
			WriteLog(L"Некорректный MBAP");
			ret = 1;
			break;
		}
		if(datasize < 2)
		{
			WriteLog(L"Размер блока данных не может быть <= 0");
			ret = 1;
			break;
		}
		if(datasize > MODBUS_MAX_PDU)
		{
			WriteLog(L"Размер блока данных запроса превышает максимально допустимый");
			ret = 1;
			break;
		}
	
		//чтение фрейма
		waitRes = recvall_ov(&clientOverlapped,wsaEvent,
			clientSock,frame_buffer,
			datasize,MODBUS_TRANS_TIMEOUT,0);
		if(waitRes == 0)
		{
			WriteLog(L"Ошибка чтения фрейма или истек тайм-аут ожидания фрейма");
			ret = 1;
			break;
		}

		switch (frame_buffer[0])
		{
		//чтение дискретных входов
		case MODBUS_READ_DISCRETE_INPUTS:
			num = ModbusReadDiscreteInputsFunc(mbap_buffer,frame_buffer);
			break;
		//чтение входных регистров
		case MODBUS_READ_INPUT_REGISTERS:
			num = ModbusReadInputRegisters(mbap_buffer,frame_buffer);
			break;
		//запись регистра состояния
		case MODBUS_WRITE_SINGLE_COIL:
			num = ModbusWriteSingleCoilFunc(mbap_buffer,frame_buffer);
		//чтение идентификационных параметров
		case MODBUS_READ_DEVICE_IDENTIFICATION:
			num = ModbusReadDeviceIdentificationFunc(mbap_buffer,frame_buffer);
		default:
			num = ModbusExceptionRsp(mbap_buffer,frame_buffer,MODBUS_ILLEGAL_FUNCTION);
		}

		memcpy_s(receive_buffer,MODBUS_MPAB_LEN,mbap_buffer,MODBUS_MPAB_LEN);
		memcpy_s(receive_buffer+MODBUS_MPAB_LEN,num,frame_buffer,num);

		waitRes = sendall_ov(&clientOverlapped,wsaEvent,
			clientSock,receive_buffer,
			MODBUS_MPAB_LEN+num,MODBUS_TRANS_TIMEOUT,0);
		if(waitRes == 0)
		{
			WriteLog(L"Ошибка в результате отправки или истек тайм-аут ожидания");
			ret = 1;
			break;
		}
		/*waitRes = sendall_ov(&clientOverlapped,wsaEvent,
			clientSock,mbap_buffer,
			MODBUS_MPAB_LEN,MODBUS_TRANS_TIMEOUT,0);
		if(waitRes == 0)
		{
			WriteLog(L"Ошибка в результате отправки MBAP или истек тайм-аут ожидания");
			ret = 1;
			break;
		}
		waitRes = sendall_ov(&clientOverlapped,wsaEvent,
			clientSock,frame_buffer,
			num,MODBUS_TRANS_TIMEOUT,0);
		if(waitRes == 0)
		{
			WriteLog(L"Ошибка в результате отправки фрейма или истек тайм-аут ожидания");
			ret = 1;
			break;
		}*/
	}
	WriteLog(L"Соединение с клиентом разорвано, SocketID = %d",clientSock);
	closesocket(clientSock);
	WSACloseEvent(wsaEvent);

	WriteLog(L"Завершение клиентского потока, ThreadID = %d",curThreadID);
	_endthreadex(ret);	
	return ret;
}

/*Функции modbus
	0x02 - чтение входных дискретных регистров 

	формат тела сообщения (байты):
		0	-	0х01
		1,2		-	стартовый регистр
		3,4		-	количество запрашиваемых регистров
*/
char ModbusReadDiscreteInputsFunc(char* mbap, char* frame)
{
	//длина, записываемая в секцию MBAP = длина фрейма + ID slave
	char mbaplen;
	//буфер регистров состояния
	char regbuf[MODBUS_MAX_DISCRETE_INPUTS];
	//количество байтов состояния регистров в ответном сообщении
	char quantity_of_bytes;

	//обработка структуры запроса

	//стартовый адрес
	u_short startaddr = GetTCPShort(&frame[1]);
	//кол-во запрашиваемых регистров
	u_short quantity = GetTCPShort(&frame[3]);

	//запрошенный диапазон регистров выходит за допустимый диапазон
	if(startaddr > MODBUS_MAX_DISCRETE_INPUTS ||
		startaddr + quantity > MODBUS_MAX_DISCRETE_INPUTS)
	{
		return ModbusExceptionRsp(mbap,frame,MODBUS_ILLEGAL_DATA_ADDRESS);
	}
	
	//количество байт выделяемой памяти данных, 1 байт - состояние 8ми каналов
	//quantity_of_bytes = ceil((float)quantity/8);

	quantity_of_bytes = BYTES_CEILING(quantity);
	

	//функция обратного вызова доступа к хранилищу

	callbacks.getdiscreteinputs_callback(regbuf,startaddr,quantity);
	
	//формирование ответного сообщения
	mbaplen = quantity_of_bytes+3;
	MakeTCPBytes(&mbap[4],mbaplen);


	//в первом байте код функции - без изменений

	//второй байт - число последующих байтов состояний
	frame[1] = quantity_of_bytes;
	//последущие байты - байты состояний регистров
	memcpy(&frame[2],regbuf,quantity_of_bytes);
	return mbaplen - 1;
}
/*
	Чтение входных 16тибитных регистров
*/
char ModbusReadInputRegisters(char* mbap, char* frame)
{
	//длина, записываемая в секцию MBAP = длина фрейма + ID slave
	char mbaplen;
	//буфер входных регистров
	char coilsbuf[MODBUS_MAX_INPUT_REGISTERS*2];
	//буфер для BigEndian значений
	char coilsbuf_swp[MODBUS_MAX_INPUT_REGISTERS*2];
	//количество байтов состояния регистров в ответном сообщении
	char quantity_of_bytes;

	//обработка структуры запроса

	//стартовый адрес
	u_short startaddr = GetTCPShort(&frame[1]);
	//кол-во запрашиваемых регистров
	u_short quantity = GetTCPShort(&frame[3]);

	//запрошенный диапазон регистров выходит за допустимый диапазон
	if(startaddr > MODBUS_MAX_INPUT_REGISTERS ||
		startaddr + quantity > MODBUS_MAX_INPUT_REGISTERS)
	{
		return ModbusExceptionRsp(mbap,frame,MODBUS_ILLEGAL_DATA_ADDRESS);
	}
	
	//количество байт выделяемой памяти данных, 2 байта - 1 регистр
	quantity_of_bytes = quantity << 1;

	//функция обратного вызова доступа к хранилищу

	callbacks.getinputregisters_callback(coilsbuf,startaddr,quantity);

	//преобразовываем принятые 16битные данные в формат BigEndian 
	//для передачи по сети
	_swab(coilsbuf,coilsbuf_swp,quantity_of_bytes);

	//формирование ответного сообщения
	mbaplen = quantity_of_bytes+3;
	MakeTCPBytes(&mbap[4],mbaplen);

	//в первом байте код функции - без изменений

	//второй байт - число последующих байтов состояний
	frame[1] = quantity_of_bytes;
	//последущие байты - байты состояний регистров
	memcpy(&frame[2],coilsbuf_swp,quantity_of_bytes);
	return mbaplen - 1;
}
/*Функции modbus
	0x05 - чтение запись отдельного флага состояния

	формат тела сообщения (байты):
		0	-	0х05
		1,2		-	адрес
		3,4		-	значение ( 0xFF или 0)
*/
char ModbusWriteSingleCoilFunc(char* mbap, char* frame)
{
	//обработка структуры запроса

	//флаг
	u_short coil = GetTCPShort(&frame[1]);
	//записываемое значение
	u_short value = GetTCPShort(&frame[3]);

	//недопустимое значение
	if(value != 0xFF && value != 0)
		return ModbusExceptionRsp(mbap,frame,MODBUS_ILLEGAL_DATA_VALUE);
	//номер флага выходит за допустимый диапазон
	if(coil >= MODBUS_MAX_COILS)
		return ModbusExceptionRsp(mbap,frame,MODBUS_ILLEGAL_DATA_ADDRESS);

	//функция обратного вызова доступа к хранилищу
	callbacks.writesinglecoil_callback(coil,value);
	//в случае нормального завершения обработки запроса ответ
	//представляет копию запроса, количество байт ответа = кол-во байт запроса
	return GET_PDU_LEN(mbap) - 1;
}

/*Функции modbus
	0x2B, MEI 0x0E - идентификация устройства
	формат тела сообщения (байты):
		0 - 0х2В  - функция
		1 - 0х0Е  - MEI 
		2 - доступ к группам объектов:
			01 - базовая идентификация (возвращаются все объекты)
			04 - специфическая идентификация (возвращается 1 объект)
		3 - ID объекта
			0 - если базовая идентификация
			0 - 2 - если специфическая идентификация
*/
char ModbusReadDeviceIdentificationFunc(char* mbap, char* frame)
{
	//длина, записываемая в секцию MBAP = длина фрейма + ID slave
	char mbaplen;

	//размер буфера MODBUS_MAX_PDU - байт функции - байт MEI
	char buffer[MODBUS_MAX_PDU - 2];

	//доступ к группам объектов
	char readDevID = frame[2];
	//ID объекта
	char objectID = frame[3];

	if(frame[1] != 0x0E)
		return 0;
	//значение Object ID за пределами допустимого
	if(objectID > 2)
		return ModbusExceptionRsp(mbap,frame,MODBUS_ILLEGAL_DATA_ADDRESS);
	if(readDevID != 1 && readDevID != 4)
		return ModbusExceptionRsp(mbap,frame,MODBUS_ILLEGAL_DATA_VALUE);
	
	mbaplen = callbacks.getdeviceid_callback(buffer,readDevID,objectID);

	return mbaplen - 1;
}

char ModbusExceptionRsp(char* mbap, char* frame,enum MODBUS_ERRORS err)
{
	MakeTCPBytes(&mbap[4],3);
	frame[0] |= 0x80;
	frame[1] = err;
	return 2;
}