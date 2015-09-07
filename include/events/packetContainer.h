/*
 * packetContainer.h
 *
 *  Created on: May 25, 2015
 *      Author: llongi
 */

#ifndef LIBCAER_EVENTS_PACKETCONTAINER_H_
#define LIBCAER_EVENTS_PACKETCONTAINER_H_

#include "common.h"

struct caer_event_packet_container {
	uint32_t eventPacketsNumber; // Number of different event packets contained.
	caerEventPacketHeader eventPackets[];
}__attribute__((__packed__));

// Keep several packets of multiple types together, for easy time-based association.
typedef struct caer_event_packet_container *caerEventPacketContainer;

static inline uint32_t caerEventPacketContainerGetEventPacketsNumber(caerEventPacketContainer header) {
	return (le32toh(header->eventPacketsNumber));
}

static inline void caerEventPacketContainerSetEventPacketsNumber(caerEventPacketContainer header,
	uint32_t eventPacketsNumber) {
	header->eventPacketsNumber = htole32(eventPacketsNumber);
}

static inline caerEventPacketContainer caerEventPacketContainerAllocate(uint32_t eventPacketsNumber) {
	size_t eventPacketContainerSize = eventPacketsNumber * sizeof(caerEventPacketContainer);

	caerEventPacketContainer packetContainer = calloc(1, eventPacketContainerSize);
	if (packetContainer == NULL) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Event Packet Container",
			"Failed to allocate %zu bytes of memory for Event Packet Container, containing %"
			PRIu32 " packets. Error: %d.", eventPacketContainerSize, eventPacketsNumber, errno);
#endif
		return (NULL);
	}

	// Fill in header fields.
	caerEventPacketContainerSetEventPacketsNumber(packetContainer, eventPacketsNumber);

	return (packetContainer);
}

static inline void *caerEventPacketContainerGetEventPacket(caerEventPacketContainer header, uint32_t n) {
	// Check that we're not out of bounds.
	if (n >= caerEventPacketContainerGetEventPacketsNumber(header)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Event Packet Container",
			"Called caerEventPacketContainerGetEventPacket() with invalid event packet offset %" PRIu32 ", while maximum allowed value is %" PRIu32 ".",
			n, caerEventPacketContainerGetEventPacketsNumber(header));
#endif
		return (NULL);
	}

	// Return a pointer to the specified event packet.
	return (header->eventPackets[n]);
}

static inline void caerEventPacketContainerSetEventPacket(caerEventPacketContainer header, uint32_t n,
	caerEventPacketHeader packetHeader) {
	// Check that we're not out of bounds.
	if (n >= caerEventPacketContainerGetEventPacketsNumber(header)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Event Packet Container",
			"Called caerEventPacketContainerSetEventPacket() with invalid event packet offset %" PRIu32 ", while maximum allowed value is %" PRIu32 ".",
			n, caerEventPacketContainerGetEventPacketsNumber(header));
#endif
		return;
	}

	// Store the given event packet.
	header->eventPackets[n] = packetHeader;
}

#endif /* LIBCAER_EVENTS_PACKETCONTAINER_H_ */
