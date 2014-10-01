#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

// Compile time log filter:

#define NODEBUG_L 0
#define ERROR_L 1
#define WARNING_L 2
#define INFO_L 3
#define DEBUG_L 4

#ifndef LOG_LEVEL
#define LOG_LEVEL INFO_L
#endif

// -------------

extern FILE *logfile;

#define OUTPUT_LOG_MSG(LEVEL, ...) do { \
	fprintf(logfile, LEVEL "mininit: " __VA_ARGS__); \
	} while (0)

#if LOG_LEVEL >= DEBUG_L
#define DEBUG(...) OUTPUT_LOG_MSG("<15>", __VA_ARGS__)
#else
#define DEBUG(...)
#endif

#if LOG_LEVEL >= INFO_L
#define INFO(...) OUTPUT_LOG_MSG("<14>", __VA_ARGS__)
#else
#define INFO(...)
#endif

#if LOG_LEVEL >= WARNING_L
#define WARNING(...) OUTPUT_LOG_MSG("<12>", "WARNING: " __VA_ARGS__)
#else
#define WARNING(...)
#endif

#if LOG_LEVEL >= ERROR_L
#define ERROR(...) OUTPUT_LOG_MSG("<11>", "ERROR: " __VA_ARGS__)
#else
#define ERROR(...)
#endif

#endif // DEBUG_H
