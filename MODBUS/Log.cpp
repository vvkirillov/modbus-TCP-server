#include "Log.h"

#define BUFSIZE 1024

//int WriteLog(const wchar_t* str)
//{
//	struct tm *lctime;
//	time_t nowtime;
//	time( &nowtime );
//	lctime = localtime( &nowtime ); 
//	
//	wprintf_s(L"%02d:%02d:%02d %s\r\n",lctime->tm_hour,
//		lctime->tm_min,lctime->tm_sec,str);
//	fflush(stdout);
//	return 0;
//}

int WriteLogWSAError(const wchar_t* str)
{
	struct tm lctime;
	time_t nowtime;
	time( &nowtime );
	localtime_s( &lctime, &nowtime ); 
	
	wprintf_s(L"%02d:%02d:%02d %s, WSAError = %d\r\n",lctime.tm_hour,
		lctime.tm_min,lctime.tm_sec,str,WSAGetLastError());
	fflush(stdout);
	return 0;
}

//int WriteLogNum(const wchar_t* format,int num)
//{
//	struct tm *lctime;
//	time_t nowtime;
//	time( &nowtime );
//	lctime = localtime( &nowtime ); 
//
//	wchar_t buf[256] = L"%02d:%02d:%02d ";
//
//	wcscat(buf,format);
//	wcscat(buf,L"\r\n");
//
//	wprintf(buf,lctime->tm_hour,
//		lctime->tm_min,lctime->tm_sec,num);
//
//	fflush(stdout);
//	return 0;
//}

int WriteLog(const wchar_t* format,...)
{
	wchar_t strbuf[BUFSIZE];
	const wchar_t TIME_STR[] = L"%02d:%02d:%02d "; 
	struct tm lctime;
	int len;
	time_t nowtime;	
	va_list args;

	time( &nowtime );
	localtime_s( &lctime, &nowtime );
	
	va_start(args,format);

	len = _vscwprintf_p( format, args ) + 1;

	wprintf_s(TIME_STR,lctime.tm_hour,
		lctime.tm_min,lctime.tm_sec);
	_vsnwprintf_s(strbuf,BUFSIZE,len,format,args);
	_putws(strbuf);

	va_end(args);

	fflush(stdout);
	return 0;
}