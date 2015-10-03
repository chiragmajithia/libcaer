/*
 * common.h
 *
 *  Created on: Nov 26, 2013
 *      Author: chtekk
 */

#ifndef LIBCAER_EVENTS_COMMON_H_
#define LIBCAER_EVENTS_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "libcaer.h"

// 0 in the 0th bit means invalid, 1 means valid.
// This way zeroing-out an event packet sets all its events to invalid.
#define VALID_MARK_SHIFT 0
#define VALID_MARK_MASK 0x00000001

// Timestamps have 31 significant bits, so TSOverflow needs to be shifted by that amount.
#define TS_OVERFLOW_SHIFT 31

enum caer_default_event_types {
	SPECIAL_EVENT = 0,
	POLARITY_EVENT = 1,
	FRAME_EVENT = 2,
	IMU6_EVENT = 3,
	IMU9_EVENT = 4,
	SAMPLE_EVENT = 5,
	EAR_EVENT = 6,
};

// Use signed integers for maximum compatibility with other languages.
struct caer_event_packet_header {
	int16_t eventType; // Numerical type ID, unique to each event type (see enum).
	int16_t eventSource; // Numerical source ID, unique inside a process.
	int32_t eventSize; // Size of one event in bytes.
	int32_t eventTSOffset; // Offset in bytes at which the main 32bit time-stamp can be found.
	int32_t eventTSOverflow; // Overflow counter for the standard 32bit event time-stamp.
	int32_t eventCapacity; // Maximum number of events this packet can store.
	int32_t eventNumber; // Total number of events present in this packet (valid + invalid).
	int32_t eventValid; // Total number of valid events present in this packet.
}__attribute__((__packed__));

typedef struct caer_event_packet_header *caerEventPacketHeader;

static inline int16_t caerEventPacketHeaderGetEventType(caerEventPacketHeader header) {
	return (le16toh(header->eventType));
}

static inline void caerEventPacketHeaderSetEventType(caerEventPacketHeader header, int16_t eventType) {
	if (eventType < 0) {
		// Negative numbers (bit 31 set) are not allowed!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "EventPacket Header", "Called caerEventPacketHeaderSetEventType() with negative value!");
#endif
		return;
	}

	header->eventType = htole16(eventType);
}

// Generic event packet freeing function. Automatically figures out event type.
void caerEventPacketFree(caerEventPacketHeader header);

static inline int16_t caerEventPacketHeaderGetEventSource(caerEventPacketHeader header) {
	return (le16toh(header->eventSource));
}

static inline void caerEventPacketHeaderSetEventSource(caerEventPacketHeader header, int16_t eventSource) {
	if (eventSource < 0) {
		// Negative numbers (bit 31 set) are not allowed!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "EventPacket Header",
			"Called caerEventPacketHeaderSetEventSource() with negative value!");
#endif
		return;
	}

	header->eventSource = htole16(eventSource);
}

static inline int32_t caerEventPacketHeaderGetEventSize(caerEventPacketHeader header) {
	return (le32toh(header->eventSize));
}

static inline void caerEventPacketHeaderSetEventSize(caerEventPacketHeader header, int32_t eventSize) {
	if (eventSize < 0) {
		// Negative numbers (bit 31 set) are not allowed!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "EventPacket Header", "Called caerEventPacketHeaderSetEventSize() with negative value!");
#endif
		return;
	}

	header->eventSize = htole32(eventSize);
}

static inline int32_t caerEventPacketHeaderGetEventTSOffset(caerEventPacketHeader header) {
	return (le32toh(header->eventTSOffset));
}

static inline void caerEventPacketHeaderSetEventTSOffset(caerEventPacketHeader header, int32_t eventTSOffset) {
	if (eventTSOffset < 0) {
		// Negative numbers (bit 31 set) are not allowed!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "EventPacket Header",
			"Called caerEventPacketHeaderSetEventTSOffset() with negative value!");
#endif
		return;
	}

	header->eventTSOffset = htole32(eventTSOffset);
}

static inline int32_t caerEventPacketHeaderGetEventTSOverflow(caerEventPacketHeader header) {
	return (le32toh(header->eventTSOverflow));
}

// Limit TSOverflow to 31 bits for compatibility with languages that have no unsigned integer (Java).
static inline void caerEventPacketHeaderSetEventTSOverflow(caerEventPacketHeader header, int32_t eventTSOverflow) {
	if (eventTSOverflow < 0) {
		// Negative numbers (bit 31 set) are not allowed!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "EventPacket Header",
			"Called caerEventPacketHeaderSetEventTSOverflow() with negative value!");
#endif
		return;
	}

	header->eventTSOverflow = htole32(eventTSOverflow);
}

static inline int32_t caerEventPacketHeaderGetEventCapacity(caerEventPacketHeader header) {
	return (le32toh(header->eventCapacity));
}

static inline void caerEventPacketHeaderSetEventCapacity(caerEventPacketHeader header, int32_t eventsCapacity) {
	if (eventsCapacity < 0) {
		// Negative numbers (bit 31 set) are not allowed!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "EventPacket Header",
			"Called caerEventPacketHeaderSetEventCapacity() with negative value!");
#endif
		return;
	}

	header->eventCapacity = htole32(eventsCapacity);
}

static inline int32_t caerEventPacketHeaderGetEventNumber(caerEventPacketHeader header) {
	return (le32toh(header->eventNumber));
}

static inline void caerEventPacketHeaderSetEventNumber(caerEventPacketHeader header, int32_t eventsNumber) {
	if (eventsNumber < 0) {
		// Negative numbers (bit 31 set) are not allowed!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "EventPacket Header",
			"Called caerEventPacketHeaderSetEventNumber() with negative value!");
#endif
		return;
	}

	header->eventNumber = htole32(eventsNumber);
}

static inline int32_t caerEventPacketHeaderGetEventValid(caerEventPacketHeader header) {
	return (le32toh(header->eventValid));
}

static inline void caerEventPacketHeaderSetEventValid(caerEventPacketHeader header, int32_t eventsValid) {
	if (eventsValid < 0) {
		// Negative numbers (bit 31 set) are not allowed!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "EventPacket Header", "Called caerEventPacketHeaderSetEventValid() with negative value!");
#endif
		return;
	}

	header->eventValid = htole32(eventsValid);
}

static inline void *caerGenericEventGetEvent(caerEventPacketHeader headerPtr, int32_t n) {
	// Check that we're not out of bounds.
	if (n < 0 || n >= caerEventPacketHeaderGetEventCapacity(headerPtr)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Generic Event",
			"Called caerGenericEventGetEvent() with invalid event offset %" PRIi32 ", while maximum allowed value is %" PRIi32 ". Negative values are not allowed!",
			n, caerEventPacketHeaderGetEventCapacity(headerPtr));
#endif
		return (NULL);
	}

	// Return a pointer to the specified event.
	return (((uint8_t *) headerPtr)
		+ (sizeof(struct caer_event_packet_header) + U64T(n * caerEventPacketHeaderGetEventSize(headerPtr))));
}

static inline int32_t caerGenericEventGetTimestamp(void *eventPtr, caerEventPacketHeader headerPtr) {
	return (le32toh(*((int32_t *) (((uint8_t *) eventPtr) + U64T(caerEventPacketHeaderGetEventTSOffset(headerPtr))))));
}

static inline int64_t caerGenericEventGetTimestamp64(void *eventPtr, caerEventPacketHeader headerPtr) {
	return (I64T((U64T(caerEventPacketHeaderGetEventTSOverflow(headerPtr)) << TS_OVERFLOW_SHIFT)
		| U64T(caerGenericEventGetTimestamp(eventPtr, headerPtr))));
}

static inline bool caerGenericEventIsValid(void *eventPtr) {
	// Look at first byte of event memory's lowest bit.
	// This should always work since first event member must contain the valid mark
	// and memory is little-endian, so lowest bit must be in first byte of memory.
	return (*((uint8_t *) eventPtr) & VALID_MARK_MASK);
}

#ifdef __cplusplus
}
#endif

#endif /* LIBCAER_EVENTS_COMMON_H_ */
