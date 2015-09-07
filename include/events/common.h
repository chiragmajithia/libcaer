/*
 * common.h
 *
 *  Created on: Nov 26, 2013
 *      Author: chtekk
 */

#ifndef LIBCAER_EVENTS_COMMON_H_
#define LIBCAER_EVENTS_COMMON_H_

#include "libcaer.h"

// 0 in the 0th bit means invalid, 1 means valid.
// This way zeroing-out an event packet sets all its events to invalid.
#define VALID_MARK_SHIFT 0
#define VALID_MARK_MASK 0x00000001

// Timestamps have 31 significant bits, so TSOverflow needs to be shifted by that amount.
#define TS_OVERFLOW_SHIFT 21

enum caer_event_types {
	SPECIAL_EVENT = 0,
	POLARITY_EVENT = 1,
	FRAME_EVENT = 2,
	IMU6_EVENT = 3,
	IMU9_EVENT = 4,
	SAMPLE_EVENT = 5,
	EAR_EVENT = 6,
};

struct caer_event_packet_header {
	uint16_t eventType; // Numerical type ID, unique to each event type (see enum).
	uint16_t eventSource; // Numerical source ID, unique inside a process.
	uint32_t eventSize; // Size of one event in bytes.
	uint32_t eventTSOffset; // Offset in bytes at which the main 32bit time-stamp can be found.
	uint32_t eventTSOverflow; // Overflow counter for the standard 32bit event time-stamp.
	uint32_t eventCapacity; // Maximum number of events this packet can store.
	uint32_t eventNumber; // Total number of events present in this packet (valid + invalid).
	uint32_t eventValid; // Total number of valid events present in this packet.
}__attribute__((__packed__));

typedef struct caer_event_packet_header *caerEventPacketHeader;

static inline uint16_t caerEventPacketHeaderGetEventType(caerEventPacketHeader header) {
	return (le16toh(header->eventType));
}

static inline void caerEventPacketHeaderSetEventType(caerEventPacketHeader header, uint16_t eventType) {
	header->eventType = htole16(eventType);
}

static inline uint16_t caerEventPacketHeaderGetEventSource(caerEventPacketHeader header) {
	return (le16toh(header->eventSource));
}

static inline void caerEventPacketHeaderSetEventSource(caerEventPacketHeader header, uint16_t eventSource) {
	header->eventSource = htole16(eventSource);
}

static inline uint32_t caerEventPacketHeaderGetEventSize(caerEventPacketHeader header) {
	return (le32toh(header->eventSize));
}

static inline void caerEventPacketHeaderSetEventSize(caerEventPacketHeader header, uint32_t eventSize) {
	header->eventSize = htole32(eventSize);
}

static inline uint32_t caerEventPacketHeaderGetEventTSOffset(caerEventPacketHeader header) {
	return (le32toh(header->eventTSOffset));
}

static inline void caerEventPacketHeaderSetEventTSOffset(caerEventPacketHeader header, uint32_t eventTSOffset) {
	header->eventTSOffset = htole32(eventTSOffset);
}

static inline uint32_t caerEventPacketHeaderGetEventTSOverflow(caerEventPacketHeader header) {
	return (le32toh(header->eventTSOverflow));
}

// Limit TSOverflow to 31 bits for compatibility with languages that have no unsigned integer (Java).
static inline void caerEventPacketHeaderSetEventTSOverflow(caerEventPacketHeader header, int32_t eventTSOverflow) {
	if (eventTSOverflow < 0) {
		// Negative means using the 31st bit!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Packet Header", "Called caerEventPacketHeaderSetEventTSOverflow() with negative value!");
#endif
		return;
	}

	header->eventTSOverflow = htole32(eventTSOverflow);
}

static inline uint32_t caerEventPacketHeaderGetEventCapacity(caerEventPacketHeader header) {
	return (le32toh(header->eventCapacity));
}

static inline void caerEventPacketHeaderSetEventCapacity(caerEventPacketHeader header, uint32_t eventsCapacity) {
	header->eventCapacity = htole32(eventsCapacity);
}

static inline uint32_t caerEventPacketHeaderGetEventNumber(caerEventPacketHeader header) {
	return (le32toh(header->eventNumber));
}

static inline void caerEventPacketHeaderSetEventNumber(caerEventPacketHeader header, uint32_t eventsNumber) {
	header->eventNumber = htole32(eventsNumber);
}

static inline uint32_t caerEventPacketHeaderGetEventValid(caerEventPacketHeader header) {
	return (le32toh(header->eventValid));
}

static inline void caerEventPacketHeaderSetEventValid(caerEventPacketHeader header, uint32_t eventsValid) {
	header->eventValid = htole32(eventsValid);
}

static inline void *caerGenericEventGetEvent(void *headerPtr, uint32_t n) {
	// Check that we're not out of bounds.
	if (n >= caerEventPacketHeaderGetEventCapacity(headerPtr)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Generic Event",
			"Called caerGenericEventGetEvent() with invalid event offset %" PRIu32 ", while maximum allowed value is %" PRIu32 ".",
			n, caerEventPacketHeaderGetEventCapacity(headerPtr));
#endif
		return (NULL);
	}

	// Return a pointer to the specified event.
	return (((uint8_t *) headerPtr)
		+ (sizeof(struct caer_event_packet_header) + (n * caerEventPacketHeaderGetEventSize(headerPtr))));
}

static inline uint32_t caerGenericEventGetTimestamp(void *eventPtr, void *headerPtr) {
	return (le32toh(*((uint32_t *) (((uint8_t *) eventPtr) + caerEventPacketHeaderGetEventTSOffset(headerPtr)))));
}

static inline uint64_t caerGenericEventGetTimestamp64(void *eventPtr, void *headerPtr) {
	return ((U64T(caerEventPacketHeaderGetEventTSOverflow(headerPtr)) << TS_OVERFLOW_SHIFT)
		| U64T(caerGenericEventGetTimestamp(eventPtr, headerPtr)));
}

static inline bool caerGenericEventIsValid(void *eventPtr) {
	return (*((uint8_t *) eventPtr) & VALID_MARK_MASK);
}

#endif /* LIBCAER_EVENTS_COMMON_H_ */
