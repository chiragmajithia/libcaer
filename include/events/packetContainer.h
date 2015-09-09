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

static inline uint32_t caerEventPacketContainerGetEventPacketsNumber(caerEventPacketContainer container) {
	return (le32toh(container->eventPacketsNumber));
}

static inline void caerEventPacketContainerSetEventPacketsNumber(caerEventPacketContainer container,
	uint32_t eventPacketsNumber) {
	container->eventPacketsNumber = htole32(eventPacketsNumber);
}

static inline caerEventPacketContainer caerEventPacketContainerAllocate(uint32_t eventPacketsNumber) {
	size_t eventPacketContainerSize = eventPacketsNumber * sizeof(caerEventPacketHeader);

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

static inline caerEventPacketHeader caerEventPacketContainerGetEventPacket(caerEventPacketContainer container,
	uint32_t n) {
	// Check that we're not out of bounds.
	if (n >= caerEventPacketContainerGetEventPacketsNumber(container)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Event Packet Container",
			"Called caerEventPacketContainerGetEventPacket() with invalid event packet offset %" PRIu32 ", while maximum allowed value is %" PRIu32 ".",
			n, caerEventPacketContainerGetEventPacketsNumber(container));
#endif
		return (NULL);
	}

	// Return a pointer to the specified event packet.
	return (container->eventPackets[n]);
}

static inline void caerEventPacketContainerSetEventPacket(caerEventPacketContainer container, uint32_t n,
	caerEventPacketHeader packetHeader) {
	// Check that we're not out of bounds.
	if (n >= caerEventPacketContainerGetEventPacketsNumber(container)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Event Packet Container",
			"Called caerEventPacketContainerSetEventPacket() with invalid event packet offset %" PRIu32 ", while maximum allowed value is %" PRIu32 ".",
			n, caerEventPacketContainerGetEventPacketsNumber(container));
#endif
		return;
	}

	// Store the given event packet.
	container->eventPackets[n] = packetHeader;
}

static inline void caerEventPacketContainerFree(caerEventPacketContainer container) {
	if (container == NULL) {
		return;
	}

	// Free packet container and ensure all subordinate memory is also freed.
	for (uint32_t i = 0; i < caerEventPacketContainerGetEventPacketsNumber(container); i++) {
		caerEventPacketHeader packetHeader = caerEventPacketContainerGetEventPacket(container, i);

		if (packetHeader != NULL) {
			caerEventPacketFree(packetHeader);
		}
	}

	free(container);
}

#endif /* LIBCAER_EVENTS_PACKETCONTAINER_H_ */
