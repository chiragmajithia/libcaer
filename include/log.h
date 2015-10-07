/*
 * log.h
 *
 *  Created on: May 25, 2015
 *      Author: llongi
 */

#ifndef LIBCAER_LOG_H_
#define LIBCAER_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Debug severity levels
#define CAER_LOG_EMERGENCY (0)
#define CAER_LOG_ALERT     (1)
#define CAER_LOG_CRITICAL  (2)
#define CAER_LOG_ERROR     (3)
#define CAER_LOG_WARNING   (4)
#define CAER_LOG_NOTICE    (5)
#define CAER_LOG_INFO      (6)
#define CAER_LOG_DEBUG     (7)

void caerLogLevelSet(uint8_t logLevel);
uint8_t caerLogLevelGet(void);
void caerLogFileDescriptorsSet(int fd1, int fd2);
void caerLog(uint8_t logLevel, const char *subSystem, const char *format, ...) __attribute__ ((format (printf, 3, 4)));

#ifdef __cplusplus
}
#endif

#endif /* LIBCAER_LOG_H_ */
