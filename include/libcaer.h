/*
 * libcaer.h
 *
 *  Created on: Oct 8, 2013
 *      Author: llongi
 */

#ifndef LIBCAER_H_
#define LIBCAER_H_

#ifdef __cplusplus
extern "C" {
#endif

// Common includes, useful for everyone.
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <endian.h>

// Include libcaer headers.
#include "log.h"

// Common macros, useful for everyone.
#define U8T(X) ((uint8_t) (X))
#define U16T(X) ((uint16_t) (X))
#define U32T(X) ((uint32_t) (X))
#define U64T(X) ((uint64_t) (X))
#define MASK_NUMBITS32(X) U32T(U32T(U32T(1) << X) - 1)
#define MASK_NUMBITS64(X) U64T(U64T(U64T(1) << X) - 1)
#define SWAP_VAR(type, x, y) { type tmpv; tmpv = (x); (x) = (y); (y) = tmpv; }

static inline bool str_equals(const char *s1, const char *s2) {
	if (s1 == NULL || s2 == NULL) {
		return (false);
	}

	if (strcmp(s1, s2) == 0) {
		return (true);
	}

	return (false);
}

static inline bool str_equals_upto(const char *s1, const char *s2, size_t len) {
	if (s1 == NULL || s2 == NULL || len == 0) {
		return (false);
	}

	if (strncmp(s1, s2, len) == 0) {
		return (true);
	}

	return (false);
}

static inline void bitArrayCopy(uint8_t *src, size_t srcPos, uint8_t *dest, size_t destPos, size_t length) {
	size_t copyOffset = 0;

	while (copyOffset < length) {
		size_t srcBytePos = (srcPos + copyOffset) >> 3;
		size_t srcBitPos = (srcPos + copyOffset) & 0x07;
		uint8_t srcBitMask = U8T(0x80 >> srcBitPos);

		size_t destBytePos = (destPos + copyOffset) >> 3;
		size_t destBitPos = (destPos + copyOffset) & 0x07;
		uint8_t destBitMask = U8T(0x80 >> destBitPos);

		if ((src[srcBytePos] & srcBitMask) != 0) {
			// Set bit.
			dest[destBytePos] |= destBitMask;
		}
		else {
			// Clear bit.
			dest[destBytePos] &= U8T(~destBitMask);
		}

		copyOffset++;
	}
}

#ifdef __cplusplus
}
#endif

#endif /* LIBCAER_H_ */
