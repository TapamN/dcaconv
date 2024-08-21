#ifndef UTIL_H
#define UTIL_H

#include <stdarg.h>

typedef enum dcaLogLevel {
	//When changing these, make sure to update logtypes[] in dcaLogLocV
	LOG_NONE,	//Disables logs, do not log to this type
	
	LOG_WARNING,	//Important warnings or errors
	LOG_COMPLETION,	//Encode completion
	LOG_PROGRESS,	//Progress of encoding
	LOG_INFO,	//Useful info on encoding
	
	LOG_ALL,	//Prints all normal user visible logs, do not log to this type
	
	LOG_DEBUG,	//Debug info. Must be the highest level, used as bounds check in pteLogLocV
} dcaLogLevel;

extern int dcaCurrentLogLevel;
void dcaLogLocV(unsigned level, const char *file, unsigned line, const char *fmt, va_list args);
void dcaLogLoc(unsigned level, const char *file, unsigned line, const char *fmt, ...);
#define dcaLog(level, ...) dcaLogLoc(level, __FILE__, __LINE__, __VA_ARGS__)


#endif
