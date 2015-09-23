/*
 * sample.h
 *
 *  Created on: Jan 6, 2014
 *      Author: llongi
 */

#ifndef LIBCAER_EVENTS_SAMPLE_H_
#define LIBCAER_EVENTS_SAMPLE_H_

#include "common.h"

#define TYPE_SHIFT 1
#define TYPE_MASK 0x0000001F
#define SAMPLE_SHIFT 8
#define SAMPLE_MASK 0x00FFFFFF

struct caer_sample_event {
	uint32_t data; // First because of valid mark.
	int32_t timestamp;
}__attribute__((__packed__));

typedef struct caer_sample_event *caerSampleEvent;

struct caer_sample_event_packet {
	struct caer_event_packet_header packetHeader;
	struct caer_sample_event events[];
}__attribute__((__packed__));

typedef struct caer_sample_event_packet *caerSampleEventPacket;

static inline caerSampleEventPacket caerSampleEventPacketAllocate(int32_t eventCapacity, int16_t eventSource,
	int32_t tsOverflow) {
	size_t eventSize = sizeof(struct caer_sample_event);
	size_t eventPacketSize = sizeof(struct caer_sample_event_packet) + ((size_t) eventCapacity * eventSize);

	// Zero out event memory (all events invalid).
	caerSampleEventPacket packet = calloc(1, eventPacketSize);
	if (packet == NULL) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Sample Event",
			"Failed to allocate %zu bytes of memory for Sample Event Packet of capacity %"
			PRIi32 " from source %" PRIi16 ". Error: %d.", eventPacketSize, eventCapacity, eventSource,
			errno);
#endif
		return (NULL);
	}

	// Fill in header fields.
	caerEventPacketHeaderSetEventType(&packet->packetHeader, SAMPLE_EVENT);
	caerEventPacketHeaderSetEventSource(&packet->packetHeader, eventSource);
	caerEventPacketHeaderSetEventSize(&packet->packetHeader, (int16_t) eventSize);
	caerEventPacketHeaderSetEventTSOffset(&packet->packetHeader, offsetof(struct caer_sample_event, timestamp));
	caerEventPacketHeaderSetEventTSOverflow(&packet->packetHeader, tsOverflow);
	caerEventPacketHeaderSetEventCapacity(&packet->packetHeader, eventCapacity);

	return (packet);
}

static inline caerSampleEvent caerSampleEventPacketGetEvent(caerSampleEventPacket packet, int32_t n) {
	// Check that we're not out of bounds.
	if (n < 0 || n >= caerEventPacketHeaderGetEventCapacity(&packet->packetHeader)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Sample Event",
			"Called caerSampleEventPacketGetEvent() with invalid event offset %" PRIi32 ", while maximum allowed value is %" PRIi32 ".",
			n, caerEventPacketHeaderGetEventCapacity(&packet->packetHeader));
#endif
		return (NULL);
	}

	// Return a pointer to the specified event.
	return (packet->events + n);
}

static inline int32_t caerSampleEventGetTimestamp(caerSampleEvent event) {
	return (le32toh(event->timestamp));
}

static inline int64_t caerSampleEventGetTimestamp64(caerSampleEvent event, caerSampleEventPacket packet) {
	return ((int64_t) ((U64T(caerEventPacketHeaderGetEventTSOverflow(&packet->packetHeader)) << TS_OVERFLOW_SHIFT)
		| U64T(caerSampleEventGetTimestamp(event))));
}

// Limit Timestamp to 31 bits for compatibility with languages that have no unsigned integer (Java).
static inline void caerSampleEventSetTimestamp(caerSampleEvent event, int32_t timestamp) {
	if (timestamp < 0) {
		// Negative means using the 31st bit!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Sample Event", "Called caerSampleEventSetTimestamp() with negative value!");
#endif
		return;
	}

	event->timestamp = htole32(timestamp);
}

static inline bool caerSampleEventIsValid(caerSampleEvent event) {
	return ((le32toh(event->data) >> VALID_MARK_SHIFT) & VALID_MARK_MASK);
}

static inline void caerSampleEventValidate(caerSampleEvent event, caerSampleEventPacket packet) {
	if (!caerSampleEventIsValid(event)) {
		event->data |= htole32(U32T(1) << VALID_MARK_SHIFT);

		// Also increase number of events and valid events.
		// Only call this on (still) invalid events!
		caerEventPacketHeaderSetEventNumber(&packet->packetHeader,
			caerEventPacketHeaderGetEventNumber(&packet->packetHeader) + 1);
		caerEventPacketHeaderSetEventValid(&packet->packetHeader,
			caerEventPacketHeaderGetEventValid(&packet->packetHeader) + 1);
	}
	else {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Sample Event", "Called caerSampleEventValidate() on already valid event.");
#endif
	}
}

static inline void caerSampleEventInvalidate(caerSampleEvent event, caerSampleEventPacket packet) {
	if (caerSampleEventIsValid(event)) {
		event->data &= htole32(~(U32T(1) << VALID_MARK_SHIFT));

		// Also decrease number of valid events. Number of total events doesn't change.
		// Only call this on valid events!
		caerEventPacketHeaderSetEventValid(&packet->packetHeader,
			caerEventPacketHeaderGetEventValid(&packet->packetHeader) - 1);
	}
	else {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Sample Event", "Called caerSampleEventInvalidate() on already invalid event.");
#endif
	}
}

static inline uint8_t caerSampleEventGetType(caerSampleEvent event) {
	return U8T((le32toh(event->data) >> TYPE_SHIFT) & TYPE_MASK);
}

static inline void caerSampleEventSetType(caerSampleEvent event, uint8_t type) {
	event->data |= htole32((U32T(type) & TYPE_MASK) << TYPE_SHIFT);
}

static inline uint32_t caerSampleEventGetSample(caerSampleEvent event) {
	return U32T((le32toh(event->data) >> SAMPLE_SHIFT) & SAMPLE_MASK);
}

static inline void caerSampleEventSetSample(caerSampleEvent event, uint32_t sample) {
	event->data |= htole32((U32T(sample) & SAMPLE_MASK) << SAMPLE_SHIFT);
}

#endif /* LIBCAER_EVENTS_SAMPLE_H_ */
