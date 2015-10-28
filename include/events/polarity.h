/**
 * @file polarity.h
 *
 * Polarity Events format definition and handling functions.
 */

#ifndef LIBCAER_EVENTS_POLARITY_H_
#define LIBCAER_EVENTS_POLARITY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

#define POLARITY_SHIFT 1
#define POLARITY_MASK 0x00000001
#define Y_ADDR_SHIFT 2
#define Y_ADDR_MASK 0x00007FFF
#define X_ADDR_SHIFT 17
#define X_ADDR_MASK 0x00007FFF

// (0, 0) is situated in the lower left corner, like in OpenGL.
struct caer_polarity_event {
	uint32_t data; // First because of valid mark.
	int32_t timestamp;
}__attribute__((__packed__));

typedef struct caer_polarity_event *caerPolarityEvent;

struct caer_polarity_event_packet {
	struct caer_event_packet_header packetHeader;
	struct caer_polarity_event events[];
}__attribute__((__packed__));

typedef struct caer_polarity_event_packet *caerPolarityEventPacket;

caerPolarityEventPacket caerPolarityEventPacketAllocate(int32_t eventCapacity, int16_t eventSource, int32_t tsOverflow);

static inline caerPolarityEvent caerPolarityEventPacketGetEvent(caerPolarityEventPacket packet, int32_t n) {
	// Check that we're not out of bounds.
	if (n < 0 || n >= caerEventPacketHeaderGetEventCapacity(&packet->packetHeader)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Polarity Event",
			"Called caerPolarityEventPacketGetEvent() with invalid event offset %" PRIi32 ", while maximum allowed value is %" PRIi32 ".",
			n, caerEventPacketHeaderGetEventCapacity(&packet->packetHeader));
#endif
		return (NULL);
	}

	// Return a pointer to the specified event.
	return (packet->events + n);
}

static inline int32_t caerPolarityEventGetTimestamp(caerPolarityEvent event) {
	return (le32toh(event->timestamp));
}

static inline int64_t caerPolarityEventGetTimestamp64(caerPolarityEvent event, caerPolarityEventPacket packet) {
	return (I64T(
		(U64T(caerEventPacketHeaderGetEventTSOverflow(&packet->packetHeader)) << TS_OVERFLOW_SHIFT) | U64T(caerPolarityEventGetTimestamp(event))));
}

// Limit Timestamp to 31 bits for compatibility with languages that have no unsigned integer (Java).
static inline void caerPolarityEventSetTimestamp(caerPolarityEvent event, int32_t timestamp) {
	if (timestamp < 0) {
		// Negative means using the 31st bit!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Polarity Event", "Called caerPolarityEventSetTimestamp() with negative value!");
#endif
		return;
	}

	event->timestamp = htole32(timestamp);
}

static inline bool caerPolarityEventIsValid(caerPolarityEvent event) {
	return ((le32toh(event->data) >> VALID_MARK_SHIFT) & VALID_MARK_MASK);
}

static inline void caerPolarityEventValidate(caerPolarityEvent event, caerPolarityEventPacket packet) {
	if (!caerPolarityEventIsValid(event)) {
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
		caerLog(CAER_LOG_CRITICAL, "Polarity Event", "Called caerPolarityEventValidate() on already valid event.");
#endif
	}
}

static inline void caerPolarityEventInvalidate(caerPolarityEvent event, caerPolarityEventPacket packet) {
	if (caerPolarityEventIsValid(event)) {
		event->data &= htole32(~(U32T(1) << VALID_MARK_SHIFT));

		// Also decrease number of valid events. Number of total events doesn't change.
		// Only call this on valid events!
		caerEventPacketHeaderSetEventValid(&packet->packetHeader,
			caerEventPacketHeaderGetEventValid(&packet->packetHeader) - 1);
	}
	else {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Polarity Event", "Called caerPolarityEventInvalidate() on already invalid event.");
#endif
	}
}

static inline bool caerPolarityEventGetPolarity(caerPolarityEvent event) {
	return ((le32toh(event->data) >> POLARITY_SHIFT) & POLARITY_MASK);
}

static inline void caerPolarityEventSetPolarity(caerPolarityEvent event, bool polarity) {
	event->data |= htole32((U32T(polarity) & POLARITY_MASK) << POLARITY_SHIFT);
}

static inline uint16_t caerPolarityEventGetY(caerPolarityEvent event) {
	return U16T((le32toh(event->data) >> Y_ADDR_SHIFT) & Y_ADDR_MASK);
}

static inline void caerPolarityEventSetY(caerPolarityEvent event, uint16_t yAddress) {
	event->data |= htole32((U32T(yAddress) & Y_ADDR_MASK) << Y_ADDR_SHIFT);
}

static inline uint16_t caerPolarityEventGetX(caerPolarityEvent event) {
	return U16T((le32toh(event->data) >> X_ADDR_SHIFT) & X_ADDR_MASK);
}

static inline void caerPolarityEventSetX(caerPolarityEvent event, uint16_t xAddress) {
	event->data |= htole32((U32T(xAddress) & X_ADDR_MASK) << X_ADDR_SHIFT);
}

#ifdef __cplusplus
}
#endif

#endif /* LIBCAER_EVENTS_POLARITY_H_ */
