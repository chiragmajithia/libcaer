/*
 * imu6.h
 *
 *  Created on: Jan 6, 2014
 *      Author: llongi
 */

#ifndef LIBCAER_EVENTS_IMU6_H_
#define LIBCAER_EVENTS_IMU6_H_

#include "common.h"

struct caer_imu6_event {
	uint32_t info; // First because of valid mark.
	int32_t timestamp;
	float accel_x;
	float accel_y;
	float accel_z;
	float gyro_x;
	float gyro_y;
	float gyro_z;
	float temp;
}__attribute__((__packed__));

typedef struct caer_imu6_event *caerIMU6Event;

struct caer_imu6_event_packet {
	struct caer_event_packet_header packetHeader;
	struct caer_imu6_event events[];
}__attribute__((__packed__));

typedef struct caer_imu6_event_packet *caerIMU6EventPacket;

static inline caerIMU6EventPacket caerIMU6EventPacketAllocate(int32_t eventCapacity, int16_t eventSource) {
	size_t eventSize = sizeof(struct caer_imu6_event);
	size_t eventPacketSize = sizeof(struct caer_imu6_event_packet) + ((size_t) eventCapacity * eventSize);

	// Zero out event memory (all events invalid).
	caerIMU6EventPacket packet = calloc(1, eventPacketSize);
	if (packet == NULL) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "IMU6 Event", "Failed to allocate %zu bytes of memory for IMU6 Event Packet of capacity %"
		PRIi32 " from source %" PRIi16 ". Error: %d.", eventPacketSize, eventCapacity, eventSource,
		errno);
#endif
		return (NULL);
	}

	// Fill in header fields.
	caerEventPacketHeaderSetEventType(&packet->packetHeader, IMU6_EVENT);
	caerEventPacketHeaderSetEventSource(&packet->packetHeader, eventSource);
	caerEventPacketHeaderSetEventSize(&packet->packetHeader, (int16_t)eventSize);
	caerEventPacketHeaderSetEventTSOffset(&packet->packetHeader, offsetof(struct caer_imu6_event, timestamp));
	caerEventPacketHeaderSetEventCapacity(&packet->packetHeader, eventCapacity);

	return (packet);
}

static inline caerIMU6Event caerIMU6EventPacketGetEvent(caerIMU6EventPacket packet, int32_t n) {
	// Check that we're not out of bounds.
	if (n < 0 || n >= caerEventPacketHeaderGetEventCapacity(&packet->packetHeader)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "IMU6 Event",
			"Called caerIMU6EventPacketGetEvent() with invalid event offset %" PRIi32 ", while maximum allowed value is %" PRIi32 ".",
			n, caerEventPacketHeaderGetEventCapacity(&packet->packetHeader));
#endif
		return (NULL);
	}

	// Return a pointer to the specified event.
	return (packet->events + n);
}

static inline int32_t caerIMU6EventGetTimestamp(caerIMU6Event event) {
	return (le32toh(event->timestamp));
}

static inline int64_t caerIMU6EventGetTimestamp64(caerIMU6Event event, caerIMU6EventPacket packet) {
	return ((int64_t) ((U64T(caerEventPacketHeaderGetEventTSOverflow(&packet->packetHeader)) << TS_OVERFLOW_SHIFT)
		| U64T(caerIMU6EventGetTimestamp(event))));
}

// Limit Timestamp to 31 bits for compatibility with languages that have no unsigned integer (Java).
static inline void caerIMU6EventSetTimestamp(caerIMU6Event event, int32_t timestamp) {
	if (timestamp < 0) {
		// Negative means using the 31st bit!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "IMU6 Event", "Called caerIMU6EventSetTimestamp() with negative value!");
#endif
		return;
	}

	event->timestamp = htole32(timestamp);
}

static inline bool caerIMU6EventIsValid(caerIMU6Event event) {
	return ((le16toh(event->info) >> VALID_MARK_SHIFT) & VALID_MARK_MASK);
}

static inline void caerIMU6EventValidate(caerIMU6Event event, caerIMU6EventPacket packet) {
	if (!caerIMU6EventIsValid(event)) {
		event->info |= htole16(U16T(1) << VALID_MARK_SHIFT);

		// Also increase number of events and valid events.
		// Only call this on (still) invalid events!
		caerEventPacketHeaderSetEventNumber(&packet->packetHeader,
			caerEventPacketHeaderGetEventNumber(&packet->packetHeader) + 1);
		caerEventPacketHeaderSetEventValid(&packet->packetHeader,
			caerEventPacketHeaderGetEventValid(&packet->packetHeader) + 1);
	}
	else {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "IMU6 Event", "Called caerIMU6EventValidate() on already valid event.");
#endif
	}
}

static inline void caerIMU6EventInvalidate(caerIMU6Event event, caerIMU6EventPacket packet) {
	if (caerIMU6EventIsValid(event)) {
		event->info &= htole16((uint16_t) (~(U16T(1) << VALID_MARK_SHIFT)));

		// Also decrease number of valid events. Number of total events doesn't change.
		// Only call this on valid events!
		caerEventPacketHeaderSetEventValid(&packet->packetHeader,
			caerEventPacketHeaderGetEventValid(&packet->packetHeader) - 1);
	}
	else {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "IMU6 Event", "Called caerIMU6EventInvalidate() on already invalid event.");
#endif
	}
}

static inline float caerIMU6EventGetAccelX(caerIMU6Event event) {
	return (le32toh(event->accel_x));
}

static inline void caerIMU6EventSetAccelX(caerIMU6Event event, float accelX) {
	event->accel_x = htole32(accelX);
}

static inline float caerIMU6EventGetAccelY(caerIMU6Event event) {
	return (le32toh(event->accel_y));
}

static inline void caerIMU6EventSetAccelY(caerIMU6Event event, float accelY) {
	event->accel_y = htole32(accelY);
}

static inline float caerIMU6EventGetAccelZ(caerIMU6Event event) {
	return (le32toh(event->accel_z));
}

static inline void caerIMU6EventSetAccelZ(caerIMU6Event event, float accelZ) {
	event->accel_z = htole32(accelZ);
}

static inline float caerIMU6EventGetGyroX(caerIMU6Event event) {
	return (le32toh(event->gyro_x));
}

static inline void caerIMU6EventSetGyroX(caerIMU6Event event, float gyroX) {
	event->gyro_x = htole32(gyroX);
}

static inline float caerIMU6EventGetGyroY(caerIMU6Event event) {
	return (le32toh(event->gyro_y));
}

static inline void caerIMU6EventSetGyroY(caerIMU6Event event, float gyroY) {
	event->gyro_y = htole32(gyroY);
}

static inline float caerIMU6EventGetGyroZ(caerIMU6Event event) {
	return (le32toh(event->gyro_z));
}

static inline void caerIMU6EventSetGyroZ(caerIMU6Event event, float gyroZ) {
	event->gyro_z = htole32(gyroZ);
}

static inline float caerIMU6EventGetTemp(caerIMU6Event event) {
	return (le32toh(event->temp));
}

static inline void caerIMU6EventSetTemp(caerIMU6Event event, float temp) {
	event->temp = htole32(temp);
}

#endif /* LIBCAER_EVENTS_IMU6_H_ */
