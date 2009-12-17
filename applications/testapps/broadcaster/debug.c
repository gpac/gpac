#include "debug.h"
#include <stdio.h>
#include <stdarg.h>

static int _broadcaster_debug = 0;

void setDebugMode(int mode)
{
	_broadcaster_debug = mode;
}

int getDebugMode()
{
	return _broadcaster_debug;
}

void dprintf(debugMode debug, const char *msg, ...)
{
	va_list ap;
	va_start( ap, msg );
	if ((debug & _broadcaster_debug) == debug)
		vfprintf(stderr, msg, ap);
	va_end(ap);
}
