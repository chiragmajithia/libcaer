/**
 * @file frame.h
 *
 * Frame Events format definition and handling functions.
 * This event type encodes intensity frames, like you would
 * get from a normal APS camera. It supports multiple channels
 * for color, as well as multiple Regions of Interest (ROI).
 * The (0, 0) pixel is in the lower left corner, like in OpenGL.
 */

#ifndef LIBCAER_EVENTS_FRAME_H_
#define LIBCAER_EVENTS_FRAME_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

/**
 * Shift and mask values for the channel number and ROI
 * identifier for the 'info' field of the frame event.
 * Multiple channels (RGB for example) are possible, up to 64 channels.
 * Also, up to 128 different Regions of Interest (ROI) can be tracked.
 * Bit 0 is the valid mark, see 'common.h' for more details.
 */
#define CHANNEL_NUMBER_SHIFT 1
#define CHANNEL_NUMBER_MASK 0x0000003F
#define ROI_IDENTIFIER_SHIFT 7
#define ROI_IDENTIFIER_MASK 0x0000007F

/**
 * Frame event data structure definition.
 * This contains the actual information on the frame (ROI, channels),
 * several timestamps to signal start and end of capture and of exposure,
 * as well as thea actual pixels, in a 16 bit normalized format.
 * The (0, 0) address is in the lower left corner, like in OpenGL.
 * Signed integers are used for fields that are to be interpreted
 * directly, for compatibility with languages that do not have
 * unsigned integer types, such as Java.
 */
struct caer_frame_event {
	// Event information (ROI region, channel number). First because of valid mark.
	uint32_t info;
	// Start of Frame (SOF) timestamp.
	int32_t ts_startframe;
	// End of Frame (EOF) timestamp.
	int32_t ts_endframe;
	// Start of Exposure (SOE) timestamp.
	int32_t ts_startexposure;
	// End of Exposure (EOE) timestamp.
	int32_t ts_endexposure;
	// X axis length in pixels.
	int32_t lengthX;
	// Y axis length in pixels.
	int32_t lengthY;
	// X axis position (lower left offset) in pixels.
	int32_t positionX;
	// Y axis position (lower left offset) in pixels.
	int32_t positionY;
	// Pixel array, 16 bit unsigned integers, normalized to 16 bit depth.
	uint16_t pixels[];
}__attribute__((__packed__));

/**
 * Type for pointer to frame event data structure.
 */
typedef struct caer_frame_event *caerFrameEvent;

/**
 * Frame event packet data structure definition.
 * EventPackets are always made up of the common packet header,
 * followed by 'eventCapacity' events. Everything has to
 * be in one contiguous memory block. Direct access to the events
 * array is not possible for Frame events. To calculate position
 * offsets, use the 'eventSize' field in the packet header.
 */
struct caer_frame_event_packet {
	// The common event packet header.
	struct caer_event_packet_header packetHeader;
	// All events follow here. Direct access to the events
	// array is not possible. To calculate position, use the
	// 'eventSize' field in the packetHeader.
}__attribute__((__packed__));

/**
 * Type for pointer to frame event packet data structure.
 */
typedef struct caer_frame_event_packet *caerFrameEventPacket;

/**
 *
 * The source for this function is available in src/events.c.
 *
 * @param eventCapacity
 * @param eventSource
 * @param tsOverflow
 * @param maxLengthX
 * @param maxLengthY
 * @param maxChannelNumber
 *
 * @return
 */
caerFrameEventPacket caerFrameEventPacketAllocate(int32_t eventCapacity, int16_t eventSource, int32_t tsOverflow,
	int32_t maxLengthX, int32_t maxLengthY, int16_t maxChannelNumber);

static inline caerFrameEvent caerFrameEventPacketGetEvent(caerFrameEventPacket packet, int32_t n) {
	// Check that we're not out of bounds.
	if (n < 0 || n >= caerEventPacketHeaderGetEventCapacity(&packet->packetHeader)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventPacketGetEvent() with invalid event offset %" PRIi32 ", while maximum allowed value is %" PRIi32 ".",
			n, caerEventPacketHeaderGetEventCapacity(&packet->packetHeader));
#endif
		return (NULL);
	}

	// Return a pointer to the specified event.
	return ((caerFrameEvent) (((uint8_t *) &packet->packetHeader)
		+ (CAER_EVENT_PACKET_HEADER_SIZE + U64T(n * caerEventPacketHeaderGetEventSize(&packet->packetHeader)))));
}

static inline int32_t caerFrameEventGetTSStartOfFrame(caerFrameEvent event) {
	return (le32toh(event->ts_startframe));
}

static inline int64_t caerFrameEventGetTSStartOfFrame64(caerFrameEvent event, caerFrameEventPacket packet) {
	// Even if frames have multiple time-stamps, it's not possible for later time-stamps to
	// be in a different TSOverflow period, since in those rare cases the event is dropped.
	return (I64T(
		(U64T(caerEventPacketHeaderGetEventTSOverflow(&packet->packetHeader)) << TS_OVERFLOW_SHIFT) | U64T(caerFrameEventGetTSStartOfFrame(event))));
}

// Limit Timestamp to 31 bits for compatibility with languages that have no unsigned integer (Java).
static inline void caerFrameEventSetTSStartOfFrame(caerFrameEvent event, int32_t startFrame) {
	if (startFrame < 0) {
		// Negative means using the 31st bit!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Frame Event", "Called caerFrameEventSetTSStartOfFrame() with negative value!");
#endif
		return;
	}

	event->ts_startframe = htole32(startFrame);
}

static inline int32_t caerFrameEventGetTSEndOfFrame(caerFrameEvent event) {
	return (le32toh(event->ts_endframe));
}

static inline int64_t caerFrameEventGetTSEndOfFrame64(caerFrameEvent event, caerFrameEventPacket packet) {
	// Even if frames have multiple time-stamps, it's not possible for later time-stamps to
	// be in a different TSOverflow period, since in those rare cases the event is dropped.
	return (I64T(
		(U64T(caerEventPacketHeaderGetEventTSOverflow(&packet->packetHeader)) << TS_OVERFLOW_SHIFT) | U64T(caerFrameEventGetTSEndOfFrame(event))));
}

// Limit Timestamp to 31 bits for compatibility with languages that have no unsigned integer (Java).
static inline void caerFrameEventSetTSEndOfFrame(caerFrameEvent event, int32_t endFrame) {
	if (endFrame < 0) {
		// Negative means using the 31st bit!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Frame Event", "Called caerFrameEventSetTSEndOfFrame() with negative value!");
#endif
		return;
	}

	event->ts_endframe = htole32(endFrame);
}

static inline int32_t caerFrameEventGetTSStartOfExposure(caerFrameEvent event) {
	return (le32toh(event->ts_startexposure));
}

static inline int64_t caerFrameEventGetTSStartOfExposure64(caerFrameEvent event, caerFrameEventPacket packet) {
	// Even if frames have multiple time-stamps, it's not possible for later time-stamps to
	// be in a different TSOverflow period, since in those rare cases the event is dropped.
	return (I64T(
		(U64T(caerEventPacketHeaderGetEventTSOverflow(&packet->packetHeader)) << TS_OVERFLOW_SHIFT) | U64T(caerFrameEventGetTSStartOfExposure(event))));
}

// Limit Timestamp to 31 bits for compatibility with languages that have no unsigned integer (Java).
static inline void caerFrameEventSetTSStartOfExposure(caerFrameEvent event, int32_t startExposure) {
	if (startExposure < 0) {
		// Negative means using the 31st bit!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Frame Event", "Called caerFrameEventSetTSStartOfExposure() with negative value!");
#endif
		return;
	}

	event->ts_startexposure = htole32(startExposure);
}

static inline int32_t caerFrameEventGetTSEndOfExposure(caerFrameEvent event) {
	return (le32toh(event->ts_endexposure));
}

static inline int64_t caerFrameEventGetTSEndOfExposure64(caerFrameEvent event, caerFrameEventPacket packet) {
	// Even if frames have multiple time-stamps, it's not possible for later time-stamps to
	// be in a different TSOverflow period, since in those rare cases the event is dropped.
	return (I64T(
		(U64T(caerEventPacketHeaderGetEventTSOverflow(&packet->packetHeader)) << TS_OVERFLOW_SHIFT) | U64T(caerFrameEventGetTSEndOfExposure(event))));
}

// Limit Timestamp to 31 bits for compatibility with languages that have no unsigned integer (Java).
static inline void caerFrameEventSetTSEndOfExposure(caerFrameEvent event, int32_t endExposure) {
	if (endExposure < 0) {
		// Negative means using the 31st bit!
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Frame Event", "Called caerFrameEventSetTSEndOfExposure() with negative value!");
#endif
		return;
	}

	event->ts_endexposure = htole32(endExposure);
}

// Exposure total length.
static inline int32_t caerFrameEventGetExposureLength(caerFrameEvent event) {
	return (caerFrameEventGetTSEndOfExposure(event) - caerFrameEventGetTSStartOfExposure(event));
}

// Median of exposure timestamp.
static inline int32_t caerFrameEventGetTimestamp(caerFrameEvent event) {
	return (caerFrameEventGetTSStartOfExposure(event) + (caerFrameEventGetExposureLength(event) / 2));
}

// Median of exposure timestamp (64bit).
static inline int64_t caerFrameEventGetTimestamp64(caerFrameEvent event, caerFrameEventPacket packet) {
	// Even if frames have multiple time-stamps, it's not possible for later time-stamps to
	// be in a different TSOverflow period, since in those rare cases the event is dropped.
	return (caerFrameEventGetTSStartOfExposure64(event, packet) + (caerFrameEventGetExposureLength(event) / 2));
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
		caerLog(CAER_LOG_CRITICAL, "Frame Event", "Called caerFrameEventValidate() on already valid event.");
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
		caerLog(CAER_LOG_CRITICAL, "Frame Event", "Called caerFrameEventInvalidate() on already invalid event.");
#endif
	}
}

static inline size_t caerFrameEventPacketGetPixelsSize(caerFrameEventPacket packet) {
	return ((size_t) caerEventPacketHeaderGetEventSize(&packet->packetHeader) - sizeof(struct caer_frame_event));
}

static inline size_t caerFrameEventPacketGetPixelsMaxIndex(caerFrameEventPacket packet) {
	return (caerFrameEventPacketGetPixelsSize(packet) / sizeof(uint16_t));
}

static inline uint8_t caerFrameEventGetROIIdentifier(caerFrameEvent event) {
	return U8T((le32toh(event->info) >> ROI_IDENTIFIER_SHIFT) & ROI_IDENTIFIER_MASK);
}

static inline void caerFrameEventSetROIIdentifier(caerFrameEvent event, uint8_t roiIdentifier) {
	event->info |= htole32((U32T(roiIdentifier) & ROI_IDENTIFIER_MASK) << ROI_IDENTIFIER_SHIFT);
}

static inline int32_t caerFrameEventGetLengthX(caerFrameEvent event) {
	return (le32toh(event->lengthX));
}

static inline int32_t caerFrameEventGetLengthY(caerFrameEvent event) {
	return (le32toh(event->lengthY));
}

static inline uint8_t caerFrameEventGetChannelNumber(caerFrameEvent event) {
	return U8T((le32toh(event->info) >> CHANNEL_NUMBER_SHIFT) & CHANNEL_NUMBER_MASK);
}

static inline void caerFrameEventSetLengthXLengthYChannelNumber(caerFrameEvent event, int32_t lengthX, int32_t lengthY,
	uint8_t channelNumber, caerFrameEventPacket packet) {
	// Verify lengths and channel number don't exceed allocated space.
	size_t neededMemory = (sizeof(uint16_t) * (size_t) lengthX * (size_t) lengthY * channelNumber);

	if (neededMemory > caerFrameEventPacketGetPixelsSize(packet)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventSetLengthXLengthYChannelNumber() with values that result in requiring %zu bytes, which exceeds the maximum allocated event size of %zu bytes.",
			neededMemory, (size_t) caerEventPacketHeaderGetEventSize(&packet->packetHeader));
#endif
		return;
	}

	event->lengthX = htole32(lengthX);
	event->lengthY = htole32(lengthY);
	event->info |= htole32((U32T(channelNumber) & CHANNEL_NUMBER_MASK) << CHANNEL_NUMBER_SHIFT);
}

static inline size_t caerFrameEventGetPixelsMaxIndex(caerFrameEvent event) {
	return ((size_t) (caerFrameEventGetLengthX(event) * caerFrameEventGetLengthY(event)
		* caerFrameEventGetChannelNumber(event)));
}

static inline size_t caerFrameEventGetPixelsSize(caerFrameEvent event) {
	return (caerFrameEventGetPixelsMaxIndex(event) * sizeof(uint16_t));
}

static inline int32_t caerFrameEventGetPositionX(caerFrameEvent event) {
	return (le32toh(event->positionX));
}

static inline void caerFrameEventSetPositionX(caerFrameEvent event, int32_t positionX) {
	event->positionX = htole32(positionX);
}

static inline int32_t caerFrameEventGetPositionY(caerFrameEvent event) {
	return (le32toh(event->positionY));
}

static inline void caerFrameEventSetPositionY(caerFrameEvent event, int32_t positionY) {
	event->positionY = htole32(positionY);
}

static inline uint16_t caerFrameEventGetPixel(caerFrameEvent event, int32_t xAddress, int32_t yAddress) {
	// Check frame bounds first.
	if (yAddress < 0 || yAddress >= caerFrameEventGetLengthY(event)) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventGetPixel() with invalid Y address of %" PRIi32 ", should be between 0 and %" PRIi32 ".",
			yAddress, caerFrameEventGetLengthY(event) - 1);
#endif
		return (0);
	}

	int32_t xLength = caerFrameEventGetLengthX(event);

	if (xAddress < 0 || xAddress >= xLength) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Frame Event",
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
		caerLog(CAER_LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventSetPixel() with invalid Y address of %" PRIi32 ", should be between 0 and %" PRIi32 ".",
			yAddress, caerFrameEventGetLengthY(event) - 1);
#endif
		return;
	}

	int32_t xLength = caerFrameEventGetLengthX(event);

	if (xAddress < 0 || xAddress >= xLength) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Frame Event",
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
		caerLog(CAER_LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventGetPixelForChannel() with invalid Y address of %" PRIi32 ", should be between 0 and %" PRIi32 ".",
			yAddress, caerFrameEventGetLengthY(event) - 1);
#endif
		return (0);
	}

	int32_t xLength = caerFrameEventGetLengthX(event);

	if (xAddress < 0 || xAddress >= xLength) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventGetPixelForChannel() with invalid X address of %" PRIi32 ", should be between 0 and %" PRIi32 ".",
			xAddress, xLength - 1);
#endif
		return (0);
	}

	uint8_t channelNumber = caerFrameEventGetChannelNumber(event);

	if (channel >= channelNumber) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Frame Event",
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
		caerLog(CAER_LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventSetPixelForChannel() with invalid Y address of %" PRIi32 ", should be between 0 and %" PRIi32 ".",
			yAddress, caerFrameEventGetLengthY(event) - 1);
#endif
		return;
	}

	int32_t xLength = caerFrameEventGetLengthX(event);

	if (xAddress < 0 || xAddress >= xLength) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Frame Event",
			"Called caerFrameEventSetPixelForChannel() with invalid X address of %" PRIi32 ", should be between 0 and %" PRIi32 ".",
			xAddress, xLength - 1);
#endif
		return;
	}

	uint8_t channelNumber = caerFrameEventGetChannelNumber(event);

	if (channel >= channelNumber) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Frame Event",
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

static inline uint16_t caerFrameEventGetPixelForChannelUnsafe(caerFrameEvent event, int32_t xAddress, int32_t yAddress,
	uint8_t channel) {
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

#ifdef __cplusplus
}
#endif

#endif /* LIBCAER_EVENTS_FRAME_H_ */
