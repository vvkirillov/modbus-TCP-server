#include "debug.h"

#ifdef _DEBUG

void strtoint(char* str,int num)
{
	int i = 0;
	for(;i<num;i++)
		str[i] -= 0x30;
}

void chartostr(char* str,int num)
{
	int i = 0;
	for(;i<num;i++)
		str[i] += 0x30;
}

#endif
