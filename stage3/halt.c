#include "halt.h"
#include "font.h"

void halt()
{
	for (;;)
		;
}

void panic(char *msg)
{
	println(msg);
	halt();
}
