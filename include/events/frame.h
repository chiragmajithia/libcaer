/*
 * frame.h
 *
 *  Created on: Jan 6, 2014
 *      Author: llongi
 */

#ifndef LIBCAER_EVENTS_FRAME_H_
#define LIBCAER_EVENTS_FRAME_H_

#include "common.h"

// All pixels are always normalized to 16bit depth.
// Multiple channels (such as RGB) are possible.
#define CHANNEL_NUMBER_SHIFT 1
#define CHANNEL_NUMBER_MASK 0x0000007F

struct caer_frame_event {
	uint32_t info; // First because of valid mark.
	int32_t ts_startframe;
	int32_t ts_endframe;
	int32_t ts_startexposure;
	int32_t ts_endexposure;
	int32_t lengthX;
	int32_t lengthY;
	uint16_t *pixels;
}__attribute__((__packed__));

typedef struct caer_frame_event *caerFrameEvent;

struct caer_frame_event_packet {
	struct caer_event_packet_header packetHeader;
	struct caer_frame_event events[];
}__attribute__((__packed__));

typedef struct caer_frame_event_packet *caerFrameEventPacket;

// Need pixel info too here, so storage requirement for pixel data can be determined.
static inline caerFrameEventPacket caerFrameEventPacketAllocate(int32_t eventCapacity, int16_t eventSource) {
	size_t eventSize = sizeof(struct caer_frame_event);
	size_t eventPacketSize = sizeof(struct caer_frame_event_packet) + ((size_t) eventCapacity * eventSize);

	// Zero out event memory (all events invalid).
	caerFrameEventPacket packet = calloc(1, eventPacketSize);
	if (packet == NULL) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event",
			"Failed to allocate %zu bytes of memory for Frame Event Packet of capacity %"
			PRIi32 " from source %" PRIi16 ". Error: %d.", eventPacketSize, eventCapacity, eventSource,
			errno);
#endif
		return (NULL);
	}

	// Fill in header fields.
	caerEventPacketHeaderSetEventType(&packet->packetHeader, FRAME_EVENT);
	caerEventPacketHeaderSetEventSource(&packet->packetHeader, eventSource);
	caerEventPacketHeaderSetEventSize(&packet->packetHeader, (int16_t) eventSize);
	caerEventPacketHeaderSetEventTSOffset(&packet->packetHeader, offsetof(struct caer_frame_event, ts_startexposure));
	caerEventPacketHeaderSetEventCapacity(&packet->packetHeader, eventCapacity);

	return (packet);
}

static inline caerFrameEvent caerFrameEventPacketGetEvent(caerFrameEventPacket packet, int32_t n) {
	// Check that we're not out of bounds.
	if (n < 0 || n >= caerEventPacketHeaderGetEventCapacity(&packet->packetHeader)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventPacketGetEvent() with invalid event offset %" PRIi32 ", while maximum allowed value is %" PRIi32 ".",
			n, caerEventPacketHeaderGetEventCapacity(&packet->packetHeader));
#endif
		return (NULL);
	}

	// Return a pointer to the specified event.
	return (packet->events + n);
}

void caerFrameEventPacketFreePixels(caerEventPacketHeader header) {
	if (header == NULL || caerEventPacketHeaderGetEventType(header) != FRAME_EVENT) {
		return;
	}

	// Frame also needs all pixel memory freed!
	for (int32_t i = 0; i < caerEventPacketHeaderGetEventNumber(header); i++) {
		caerFrameEvent frame = caerFrameEventPacketGetEvent((caerFrameEventPacket) header, i);

		if (frame != NULL && frame->pixels != NULL) {
			free(frame->pixels);
			frame->pixels = NULL;
		}
	}
}

// Allocate effective pixel memory for frame event.
static inline void caerFrameEventAllocatePixels(caerFrameEvent frameEvent, int32_t lengthX, int32_t lengthY,
	uint8_t channelNumber) {
	size_t pixelSize = sizeof(uint16_t) * (size_t) lengthX * (size_t) lengthY * channelNumber;

	uint16_t *pixels = calloc(1, pixelSize);
	if (pixels == NULL) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event", "Failed to allocate %zu bytes of memory for pixels. Error: %d.", pixelSize,
		errno);
#endif
		return;
	}

	// Fill in header fields.
	frameEvent->info |= htole32((U32T(channelNumber) & CHANNEL_NUMBER_MASK) << CHANNEL_NUMBER_SHIFT);
	frameEvent->lengthX = htole32(lengthX);
	frameEvent->lengthY = htole32(lengthY);
	frameEvent->pixels = pixels;
}

static inline int32_t caerFrameEventGetTSStartOfFrame(caerFrameEvent event) {
	return (le32toh(event->ts_startframe));
}

static inline int64_t caerFrameEventGetTSStartOfFrame64(caerFrameEvent event, caerFrameEventPacket packet) {
	return ((int64_t) ((U64T(caerEventPacketHeaderGetEventTSOverflow(&packet->packetHeader)) << TS_OVERFLOW_SHIFT)
		| U64T(caerFrameEventGetTSStartOfFrame(event))));
}

// Limit Timestamp to 31 bits for compatibility with languages that have no unsigned integer (Java).
static inline void caerFrameEventSetTSStartOfFrame(caerFrameEvent event, int32_t startFrame) {
	if (startFrame < 0) {
		// Negative means using the 31st bit!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event", "Called caerFrameEventSetTSStartOfFrame() with negative value!");
#endif
		return;
	}

	event->ts_startframe = htole32(startFrame);
}

static inline int32_t caerFrameEventGetTSEndOfFrame(caerFrameEvent event) {
	return (le32toh(event->ts_endframe));
}

static inline int64_t caerFrameEventGetTSEndOfFrame64(caerFrameEvent event, caerFrameEventPacket packet) {
	// Since frames have multiple time-stamps, it's possible for later time-stamps to
	// be in a different TSOverflow period (for the last frame of a packet). We can
	// detect this here and act accordingly.
	if (caerFrameEventGetTSEndOfFrame(event) >= caerFrameEventGetTSStartOfFrame(event)) {
		return ((int64_t) ((U64T(caerEventPacketHeaderGetEventTSOverflow(&packet->packetHeader)) << TS_OVERFLOW_SHIFT)
			| U64T(caerFrameEventGetTSEndOfFrame(event))));
	}
	else {
		return ((int64_t) (((U64T(caerEventPacketHeaderGetEventTSOverflow(&packet->packetHeader)) + 1)
			<< TS_OVERFLOW_SHIFT) | U64T(caerFrameEventGetTSEndOfFrame(event))));
	}
}

// Limit Timestamp to 31 bits for compatibility with languages that have no unsigned integer (Java).
static inline void caerFrameEventSetTSEndOfFrame(caerFrameEvent event, int32_t endFrame) {
	if (endFrame < 0) {
		// Negative means using the 31st bit!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event", "Called caerFrameEventSetTSEndOfFrame() with negative value!");
#endif
		return;
	}

	event->ts_endframe = htole32(endFrame);
}

static inline int32_t caerFrameEventGetTSStartOfExposure(caerFrameEvent event) {
	return (le32toh(event->ts_startexposure));
}

static inline int64_t caerFrameEventGetTSStartOfExposure64(caerFrameEvent event, caerFrameEventPacket packet) {
	// Since frames have multiple time-stamps, it's possible for later time-stamps to
	// be in a different TSOverflow period (for the last frame of a packet). We can
	// detect this here and act accordingly.
	if (caerFrameEventGetTSStartOfExposure(event) >= caerFrameEventGetTSStartOfFrame(event)) {
		return ((int64_t) ((U64T(caerEventPacketHeaderGetEventTSOverflow(&packet->packetHeader)) << TS_OVERFLOW_SHIFT)
			| U64T(caerFrameEventGetTSStartOfExposure(event))));
	}
	else {
		return ((int64_t) (((U64T(caerEventPacketHeaderGetEventTSOverflow(&packet->packetHeader)) + 1)
			<< TS_OVERFLOW_SHIFT) | U64T(caerFrameEventGetTSStartOfExposure(event))));
	}
}

// Limit Timestamp to 31 bits for compatibility with languages that have no unsigned integer (Java).
static inline void caerFrameEventSetTSStartOfExposure(caerFrameEvent event, int32_t startExposure) {
	if (startExposure < 0) {
		// Negative means using the 31st bit!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event", "Called caerFrameEventSetTSStartOfExposure() with negative value!");
#endif
		return;
	}

	event->ts_startexposure = htole32(startExposure);
}

static inline int32_t caerFrameEventGetTSEndOfExposure(caerFrameEvent event) {
	return (le32toh(event->ts_endexposure));
}

static inline int64_t caerFrameEventGetTSEndOfExposure64(caerFrameEvent event, caerFrameEventPacket packet) {
	// Since frames have multiple time-stamps, it's possible for later time-stamps to
	// be in a different TSOverflow period (for the last frame of a packet). We can
	// detect this here and act accordingly.
	if (caerFrameEventGetTSEndOfExposure(event) >= caerFrameEventGetTSStartOfFrame(event)) {
		return ((int64_t) ((U64T(caerEventPacketHeaderGetEventTSOverflow(&packet->packetHeader)) << TS_OVERFLOW_SHIFT)
			| U64T(caerFrameEventGetTSEndOfExposure(event))));
	}
	else {
		return ((int64_t) (((U64T(caerEventPacketHeaderGetEventTSOverflow(&packet->packetHeader)) + 1)
			<< TS_OVERFLOW_SHIFT) | U64T(caerFrameEventGetTSEndOfExposure(event))));
	}
}

// Limit Timestamp to 31 bits for compatibility with languages that have no unsigned integer (Java).
static inline void caerFrameEventSetTSEndOfExposure(caerFrameEvent event, int32_t endExposure) {
	if (endExposure < 0) {
		// Negative means using the 31st bit!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event", "Called caerFrameEventSetTSEndOfExposure() with negative value!");
#endif
		return;
	}

	event->ts_endexposure = htole32(endExposure);
}

static inline bool caerFrameEventIsValid(caerFrameEvent event) {
	return ((le32toh(event->info) >> VALID_MARK_SHIFT) & VALID_MARK_MASK);
}

static inline void caerFrameEventValidate(caerFrameEvent event, caerFrameEventPacket packet) {
	if (!caerFrameEventIsValid(event)) {
		event->info |= htole32(U32T(1) << VALID_MARK_SHIFT);

		// Also increase number of events and valid events.
		// Only call this on (still) invalid events!
		caerEventPacketHeaderSetEventNumber(&packet->packetHeader,
			caerEventPacketHeaderGetEventNumber(&packet->packetHeader) + 1);
		caerEventPacketHeaderSetEventValid(&packet->packetHeader,
			caerEventPacketHeaderGetEventValid(&packet->packetHeader) + 1);
	}
	else {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event", "Called caerFrameEventValidate() on already valid event.");
#endif
	}
}

static inline void caerFrameEventInvalidate(caerFrameEvent event, caerFrameEventPacket packet) {
	if (caerFrameEventIsValid(event)) {
		event->info &= htole32(~(U32T(1) << VALID_MARK_SHIFT));

		// Also decrease number of valid events. Number of total events doesn't change.
		// Only call this on valid events!
		caerEventPacketHeaderSetEventValid(&packet->packetHeader,
			caerEventPacketHeaderGetEventValid(&packet->packetHeader) - 1);
	}
	else {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event", "Called caerFrameEventInvalidate() on already invalid event.");
#endif
	}
}

static inline uint8_t caerFrameEventGetChannelNumber(caerFrameEvent event) {
	return U8T((le32toh(event->info) >> CHANNEL_NUMBER_SHIFT) & CHANNEL_NUMBER_MASK);
}

static inline int32_t caerFrameEventGetLengthX(caerFrameEvent event) {
	return (le32toh(event->lengthX));
}

static inline int32_t caerFrameEventGetLengthY(caerFrameEvent event) {
	return (le32toh(event->lengthY));
}

static inline uint16_t caerFrameEventGetPixel(caerFrameEvent event, int32_t xAddress, int32_t yAddress) {
	// Check frame bounds first.
	if (yAddress < 0 || yAddress >= caerFrameEventGetLengthY(event)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventGetPixel() with invalid Y address of %" PRIi32 ", should be between 0 and %" PRIi32 ".",
			yAddress, caerFrameEventGetLengthY(event) - 1);
#endif
		return (0);
	}

	int32_t xLength = caerFrameEventGetLengthX(event);

	if (xAddress < 0 || xAddress >= xLength) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventGetPixel() with invalid X address of %" PRIi32 ", should be between 0 and %" PRIi32".",
			xAddress, xLength - 1);
#endif
		return (0);
	}

	// Get pixel value at specified position.
	return (le16toh(event->pixels[(yAddress * xLength) + xAddress]));
}

static inline void caerFrameEventSetPixel(caerFrameEvent event, int32_t xAddress, int32_t yAddress, uint16_t pixelValue) {
	// Check frame bounds first.
	if (yAddress < 0 || yAddress >= caerFrameEventGetLengthY(event)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventSetPixel() with invalid Y address of %" PRIi32 ", should be between 0 and %" PRIi32 ".",
			yAddress, caerFrameEventGetLengthY(event) - 1);
#endif
		return;
	}

	int32_t xLength = caerFrameEventGetLengthX(event);

	if (xAddress < 0 || xAddress >= xLength) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventSetPixel() with invalid X address of %" PRIi32 ", should be between 0 and %" PRIi32 ".",
			xAddress, xLength - 1);
#endif
		return;
	}

	// Set pixel value at specified position.
	event->pixels[(yAddress * xLength) + xAddress] = htole16(pixelValue);
}

static inline uint16_t caerFrameEventGetPixelForChannel(caerFrameEvent event, int32_t xAddress, int32_t yAddress,
	uint8_t channel) {
	// Check frame bounds first.
	if (yAddress < 0 || yAddress >= caerFrameEventGetLengthY(event)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventGetPixelForChannel() with invalid Y address of %" PRIi32 ", should be between 0 and %" PRIi32 ".",
			yAddress, caerFrameEventGetLengthY(event) - 1);
#endif
		return (0);
	}

	int32_t xLength = caerFrameEventGetLengthX(event);

	if (xAddress < 0 || xAddress >= xLength) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventGetPixelForChannel() with invalid X address of %" PRIi32 ", should be between 0 and %" PRIi32 ".",
			xAddress, xLength - 1);
#endif
		return (0);
	}

	uint8_t channelNumber = caerFrameEventGetChannelNumber(event);

	if (channel >= channelNumber) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventGetPixelForChannel() with invalid channel number of %" PRIu8 ", should be between 0 and %" PRIu8 ".",
			channel, channelNumber - 1);
#endif
		return (0);
	}

	// Get pixel value at specified position.
	return (le16toh(event->pixels[(((yAddress * xLength) + xAddress) * channelNumber) + channel]));
}

static inline void caerFrameEventSetPixelForChannel(caerFrameEvent event, int32_t xAddress, int32_t yAddress,
	uint8_t channel, uint16_t pixelValue) {
	// Check frame bounds first.
	if (yAddress < 0 || yAddress >= caerFrameEventGetLengthY(event)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventSetPixelForChannel() with invalid Y address of %" PRIi32 ", should be between 0 and %" PRIi32 ".",
			yAddress, caerFrameEventGetLengthY(event) - 1);
#endif
		return;
	}

	int32_t xLength = caerFrameEventGetLengthX(event);

	if (xAddress < 0 || xAddress >= xLength) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventSetPixelForChannel() with invalid X address of %" PRIi32 ", should be between 0 and %" PRIi32 ".",
			xAddress, xLength - 1);
#endif
		return;
	}

	uint8_t channelNumber = caerFrameEventGetChannelNumber(event);

	if (channel >= channelNumber) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventSetPixelForChannel() with invalid channel number of %" PRIu8 ", should be between 0 and %" PRIu8 ".",
			channel, channelNumber - 1);
#endif
		return;
	}

	// Set pixel value at specified position.
	event->pixels[(((yAddress * xLength) + xAddress) * channelNumber) + channel] = htole16(pixelValue);
}

static inline uint16_t caerFrameEventGetPixelUnsafe(caerFrameEvent event, int32_t xAddress, int32_t yAddress) {
	// Get pixel value at specified position.
	return (le16toh(event->pixels[(yAddress * caerFrameEventGetLengthX(event)) + xAddress]));
}

static inline void caerFrameEventSetPixelUnsafe(caerFrameEvent event, int32_t xAddress, int32_t yAddress,
	uint16_t pixelValue) {
	// Set pixel value at specified position.
	event->pixels[(yAddress * caerFrameEventGetLengthX(event)) + xAddress] = htole16(pixelValue);
}

static inline uint16_t caerFrameEventGetPixelForChannelUnsafe(caerFrameEvent event, int32_t xAddress,
	int32_t yAddress, uint8_t channel) {
	// Get pixel value at specified position.
	return (le16toh(
		event->pixels[(((yAddress * caerFrameEventGetLengthX(event)) + xAddress) * caerFrameEventGetChannelNumber(event))
			+ channel]));
}

static inline void caerFrameEventSetPixelForChannelUnsafe(caerFrameEvent event, int32_t xAddress, int32_t yAddress,
	uint8_t channel, uint16_t pixelValue) {
	// Set pixel value at specified position.
	event->pixels[(((yAddress * caerFrameEventGetLengthX(event)) + xAddress) * caerFrameEventGetChannelNumber(event))
		+ channel] = htole16(pixelValue);
}

// Direct access to underlying memory. Remember the uint16_t's are little-endian!
static inline uint16_t *caerFrameEventGetPixelArrayUnsafe(caerFrameEvent event) {
	// Get pixel array.
	return (event->pixels);
}

#endif /* LIBCAER_EVENTS_FRAME_H_ */
