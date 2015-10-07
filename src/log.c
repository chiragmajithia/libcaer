#include "libcaer.h"
#include <stdatomic.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

static atomic_uint_fast8_t caerLogLevel = ATOMIC_VAR_INIT(CAER_LOG_ERROR);
static atomic_int caerLogFileDescriptor1 = ATOMIC_VAR_INIT(STDERR_FILENO);
static atomic_int caerLogFileDescriptor2 = ATOMIC_VAR_INIT(-1);

void caerLogLevelSet(uint8_t logLevel) {
	atomic_store(&caerLogLevel, logLevel);
}

uint8_t caerLogLevelGet(void) {
	return (atomic_load(&caerLogLevel));
}

void caerLogFileDescriptorsSet(int fd1, int fd2) {
	atomic_store(&caerLogFileDescriptor1, fd1);
	atomic_store(&caerLogFileDescriptor2, fd2);
}

void caerLog(uint8_t logLevel, const char *subSystem, const char *format, ...) {
	// Check that subSystem and format are defined correctly.
	if (subSystem == NULL || format == NULL) {
		caerLog(CAER_LOG_ERROR, "Logger", "Missing subSystem or format strings. Neither can be NULL.");
		return;
	}

	// Only log messages above the specified level.
	if (logLevel <= atomic_load(&caerLogLevel)) {
		// First prepend the time.
		time_t currentTimeEpoch = time(NULL);

		// From localtime_r() man-page: "According to POSIX.1-2004, localtime()
		// is required to behave as though tzset(3) was called, while
		// localtime_r() does not have this requirement."
		// So we make sure to call it here, to be portable.
		tzset();

		struct tm currentTime;
		localtime_r(&currentTimeEpoch, &currentTime);

		// Following time format uses exactly 19 characters (5 separators,
		// 4 year, 2 month, 2 day, 2 hours, 2 minutes, 2 seconds).
		size_t currentTimeStringLength = 19;
		char currentTimeString[currentTimeStringLength + 1]; // + 1 for terminating NUL byte.
		strftime(currentTimeString, currentTimeStringLength + 1, "%Y-%m-%d %H:%M:%S", &currentTime);

		// Prepend debug level as a string to format.
		const char *logLevelString;
		switch (logLevel) {
			case CAER_LOG_EMERGENCY:
				logLevelString = "EMERGENCY";
				break;

			case CAER_LOG_ALERT:
				logLevelString = "ALERT";
				break;

			case CAER_LOG_CRITICAL:
				logLevelString = "CRITICAL";
				break;

			case CAER_LOG_ERROR:
				logLevelString = "ERROR";
				break;

			case CAER_LOG_WARNING:
				logLevelString = "WARNING";
				break;

			case CAER_LOG_NOTICE:
				logLevelString = "NOTICE";
				break;

			case CAER_LOG_INFO:
				logLevelString = "INFO";
				break;

			case CAER_LOG_DEBUG:
				logLevelString = "DEBUG";
				break;

			default:
				logLevelString = "UNKNOWN";
				break;
		}

		// Copy all strings into one and ensure NUL termination.
		size_t logLength = (size_t) snprintf(NULL, 0, "%s: %s: %s: %s\n", currentTimeString, logLevelString, subSystem,
			format);
		char logString[logLength + 1];
		snprintf(logString, logLength + 1, "%s: %s: %s: %s\n", currentTimeString, logLevelString, subSystem, format);

		va_list argptr;

		int logFileDescriptor1 = atomic_load(&caerLogFileDescriptor1);
		if (logFileDescriptor1 >= 0) {
			va_start(argptr, format);
			vdprintf(logFileDescriptor1, logString, argptr);
			va_end(argptr);
		}

		int logFileDescriptor2 = atomic_load(&caerLogFileDescriptor2);
		if (logFileDescriptor2 >= 0) {
			va_start(argptr, format);
			vdprintf(logFileDescriptor2, logString, argptr);
			va_end(argptr);
		}
	}
}
