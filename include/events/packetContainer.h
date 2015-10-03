/*
 * packetContainer.h
 *
 *  Created on: May 25, 2015
 *      Author: llongi
 */

#ifndef LIBCAER_EVENTS_PACKETCONTAINER_H_
#define LIBCAER_EVENTS_PACKETCONTAINER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

// Use signed integers for maximum compatibility with other languages.
struct caer_event_packet_container {
	int32_t eventPacketsNumber; // Number of different event packets contained.
	caerEventPacketHeader eventPackets[];
}__attribute__((__packed__));

// Keep several packets of multiple types together, for easy time-based association.
typedef struct caer_event_packet_container *caerEventPacketContainer;

caerEventPacketContainer caerEventPacketContainerAllocate(int32_t eventPacketsNumber);

void caerEventPacketContainerFree(caerEventPacketContainer container);

static inline int32_t caerEventPacketContainerGetEventPacketsNumber(caerEventPacketContainer container) {
	return (le32toh(container->eventPacketsNumber));
}

static inline void caerEventPacketContainerSetEventPacketsNumber(caerEventPacketContainer container,
	int32_t eventPacketsNumber) {
	if (eventPacketsNumber < 0) {
		// Negative numbers (bit 31 set) are not allowed!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "EventPacket Container",
			"Called caerEventPacketContainerSetEventPacketsNumber() with negative value!");
#endif
		return;
	}

	container->eventPacketsNumber = htole32(eventPacketsNumber);
}

static inline caerEventPacketHeader caerEventPacketContainerGetEventPacket(caerEventPacketContainer container,
	int32_t n) {
	// Check that we're not out of bounds.
	if (n < 0 || n >= caerEventPacketContainerGetEventPacketsNumber(container)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "EventPacket Container",
			"Called caerEventPacketContainerGetEventPacket() with invalid event offset %" PRIi32 ", while maximum allowed value is %" PRIi32 ". Negative values are not allowed!",
			n, caerEventPacketContainerGetEventPacketsNumber(container));
#endif
		return (NULL);
	}

	// Return a pointer to the specified event packet.
	return (container->eventPackets[n]);
}

static inline void caerEventPacketContainerSetEventPacket(caerEventPacketContainer container, int32_t n,
	caerEventPacketHeader packetHeader) {
	// Check that we're not out of bounds.
	if (n < 0 || n >= caerEventPacketContainerGetEventPacketsNumber(container)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "EventPacket Container",
			"Called caerEventPacketContainerSetEventPacket() with invalid event offset %" PRIi32 ", while maximum allowed value is %" PRIi32 ". Negative values are not allowed!",
			n, caerEventPacketContainerGetEventPacketsNumber(container));
#endif
		return;
	}

	// Store the given event packet.
	container->eventPackets[n] = packetHeader;
}

#ifdef __cplusplus
}
#endif

#endif /* LIBCAER_EVENTS_PACKETCONTAINER_H_ */
