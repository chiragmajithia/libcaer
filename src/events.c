#include "events/packetContainer.h"
#include "events/special.h"
#include "events/polarity.h"
#include "events/frame.h"
#include "events/imu6.h"
#include "events/imu9.h"
#include "events/sample.h"
#include "events/ear.h"
#include "events/config.h"

caerEventPacketContainer caerEventPacketContainerAllocate(int32_t eventPacketsNumber) {
	size_t eventPacketContainerSize = sizeof(struct caer_event_packet_container)
		+ ((size_t) eventPacketsNumber * sizeof(caerEventPacketHeader));

	caerEventPacketContainer packetContainer = calloc(1, eventPacketContainerSize);
	if (packetContainer == NULL) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "EventPacket Container",
			"Failed to allocate %zu bytes of memory for Event Packet Container, containing %"
			PRIi32 " packets. Error: %d.", eventPacketContainerSize, eventPacketsNumber, errno);
#endif
		return (NULL);
	}

	// Fill in header fields.
	caerEventPacketContainerSetEventPacketsNumber(packetContainer, eventPacketsNumber);

	return (packetContainer);
}

void caerEventPacketContainerFree(caerEventPacketContainer container) {
	if (container == NULL) {
		return;
	}

	// Free packet container and ensure all subordinate memory is also freed.
	for (int32_t i = 0; i < caerEventPacketContainerGetEventPacketsNumber(container); i++) {
		caerEventPacketHeader packetHeader = caerEventPacketContainerGetEventPacket(container, i);

		if (packetHeader != NULL) {
			free(packetHeader);
		}
	}

	free(container);
}

caerSpecialEventPacket caerSpecialEventPacketAllocate(int32_t eventCapacity, int16_t eventSource, int32_t tsOverflow) {
	size_t eventSize = sizeof(struct caer_special_event);
	size_t eventPacketSize = sizeof(struct caer_special_event_packet) + ((size_t) eventCapacity * eventSize);

	// Zero out event memory (all events invalid).
	caerSpecialEventPacket packet = calloc(1, eventPacketSize);
	if (packet == NULL) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Special Event",
			"Failed to allocate %zu bytes of memory for Special Event Packet of capacity %"
			PRIi32 " from source %" PRIi16 ". Error: %d.", eventPacketSize, eventCapacity, eventSource,
			errno);
#endif
		return (NULL);
	}

	// Fill in header fields.
	caerEventPacketHeaderSetEventType(&packet->packetHeader, SPECIAL_EVENT);
	caerEventPacketHeaderSetEventSource(&packet->packetHeader, eventSource);
	caerEventPacketHeaderSetEventSize(&packet->packetHeader, I32T(eventSize));
	caerEventPacketHeaderSetEventTSOffset(&packet->packetHeader, offsetof(struct caer_special_event, timestamp));
	caerEventPacketHeaderSetEventTSOverflow(&packet->packetHeader, tsOverflow);
	caerEventPacketHeaderSetEventCapacity(&packet->packetHeader, eventCapacity);

	return (packet);
}

caerPolarityEventPacket caerPolarityEventPacketAllocate(int32_t eventCapacity, int16_t eventSource, int32_t tsOverflow) {
	size_t eventSize = sizeof(struct caer_polarity_event);
	size_t eventPacketSize = sizeof(struct caer_polarity_event_packet) + ((size_t) eventCapacity * eventSize);

	// Zero out event memory (all events invalid).
	caerPolarityEventPacket packet = calloc(1, eventPacketSize);
	if (packet == NULL) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Polarity Event",
			"Failed to allocate %zu bytes of memory for Polarity Event Packet of capacity %"
			PRIi32 " from source %" PRIi16 ". Error: %d.", eventPacketSize, eventCapacity, eventSource,
			errno);
#endif
		return (NULL);
	}

	// Fill in header fields.
	caerEventPacketHeaderSetEventType(&packet->packetHeader, POLARITY_EVENT);
	caerEventPacketHeaderSetEventSource(&packet->packetHeader, eventSource);
	caerEventPacketHeaderSetEventSize(&packet->packetHeader, I32T(eventSize));
	caerEventPacketHeaderSetEventTSOffset(&packet->packetHeader, offsetof(struct caer_polarity_event, timestamp));
	caerEventPacketHeaderSetEventTSOverflow(&packet->packetHeader, tsOverflow);
	caerEventPacketHeaderSetEventCapacity(&packet->packetHeader, eventCapacity);

	return (packet);
}

caerFrameEventPacket caerFrameEventPacketAllocate(int32_t eventCapacity, int16_t eventSource, int32_t tsOverflow,
	int32_t maxLengthX, int32_t maxLengthY, int16_t maxChannelNumber) {
	size_t pixelSize = sizeof(uint16_t) * (size_t) maxLengthX * (size_t) maxLengthY * (size_t) maxChannelNumber;
	size_t eventSize = sizeof(struct caer_frame_event) + pixelSize;
	size_t eventPacketSize = sizeof(struct caer_frame_event_packet) + ((size_t) eventCapacity * eventSize);

	// Zero out event memory (all events invalid).
	caerFrameEventPacket packet = calloc(1, eventPacketSize);
	if (packet == NULL) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Frame Event",
			"Failed to allocate %zu bytes of memory for Frame Event Packet of capacity %"
			PRIi32 " from source %" PRIi16 ". Error: %d.", eventPacketSize, eventCapacity, eventSource,
			errno);
#endif
		return (NULL);
	}

	// Fill in header fields.
	caerEventPacketHeaderSetEventType(&packet->packetHeader, FRAME_EVENT);
	caerEventPacketHeaderSetEventSource(&packet->packetHeader, eventSource);
	caerEventPacketHeaderSetEventSize(&packet->packetHeader, I32T(eventSize));
	caerEventPacketHeaderSetEventTSOffset(&packet->packetHeader, offsetof(struct caer_frame_event, ts_startexposure));
	caerEventPacketHeaderSetEventTSOverflow(&packet->packetHeader, tsOverflow);
	caerEventPacketHeaderSetEventCapacity(&packet->packetHeader, eventCapacity);

	return (packet);
}

caerIMU6EventPacket caerIMU6EventPacketAllocate(int32_t eventCapacity, int16_t eventSource, int32_t tsOverflow) {
	size_t eventSize = sizeof(struct caer_imu6_event);
	size_t eventPacketSize = sizeof(struct caer_imu6_event_packet) + ((size_t) eventCapacity * eventSize);

	// Zero out event memory (all events invalid).
	caerIMU6EventPacket packet = calloc(1, eventPacketSize);
	if (packet == NULL) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "IMU6 Event",
			"Failed to allocate %zu bytes of memory for IMU6 Event Packet of capacity %"
			PRIi32 " from source %" PRIi16 ". Error: %d.", eventPacketSize, eventCapacity, eventSource,
			errno);
#endif
		return (NULL);
	}

	// Fill in header fields.
	caerEventPacketHeaderSetEventType(&packet->packetHeader, IMU6_EVENT);
	caerEventPacketHeaderSetEventSource(&packet->packetHeader, eventSource);
	caerEventPacketHeaderSetEventSize(&packet->packetHeader, I32T(eventSize));
	caerEventPacketHeaderSetEventTSOffset(&packet->packetHeader, offsetof(struct caer_imu6_event, timestamp));
	caerEventPacketHeaderSetEventTSOverflow(&packet->packetHeader, tsOverflow);
	caerEventPacketHeaderSetEventCapacity(&packet->packetHeader, eventCapacity);

	return (packet);
}

caerIMU9EventPacket caerIMU9EventPacketAllocate(int32_t eventCapacity, int16_t eventSource, int32_t tsOverflow) {
	size_t eventSize = sizeof(struct caer_imu9_event);
	size_t eventPacketSize = sizeof(struct caer_imu9_event_packet) + ((size_t) eventCapacity * eventSize);

	// Zero out event memory (all events invalid).
	caerIMU9EventPacket packet = calloc(1, eventPacketSize);
	if (packet == NULL) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "IMU9 Event",
			"Failed to allocate %zu bytes of memory for IMU9 Event Packet of capacity %"
			PRIi32 " from source %" PRIi16 ". Error: %d.", eventPacketSize, eventCapacity, eventSource,
			errno);
#endif
		return (NULL);
	}

	// Fill in header fields.
	caerEventPacketHeaderSetEventType(&packet->packetHeader, IMU9_EVENT);
	caerEventPacketHeaderSetEventSource(&packet->packetHeader, eventSource);
	caerEventPacketHeaderSetEventSize(&packet->packetHeader, I32T(eventSize));
	caerEventPacketHeaderSetEventTSOffset(&packet->packetHeader, offsetof(struct caer_imu9_event, timestamp));
	caerEventPacketHeaderSetEventTSOverflow(&packet->packetHeader, tsOverflow);
	caerEventPacketHeaderSetEventCapacity(&packet->packetHeader, eventCapacity);

	return (packet);
}

caerSampleEventPacket caerSampleEventPacketAllocate(int32_t eventCapacity, int16_t eventSource, int32_t tsOverflow) {
	size_t eventSize = sizeof(struct caer_sample_event);
	size_t eventPacketSize = sizeof(struct caer_sample_event_packet) + ((size_t) eventCapacity * eventSize);

	// Zero out event memory (all events invalid).
	caerSampleEventPacket packet = calloc(1, eventPacketSize);
	if (packet == NULL) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Sample Event",
			"Failed to allocate %zu bytes of memory for Sample Event Packet of capacity %"
			PRIi32 " from source %" PRIi16 ". Error: %d.", eventPacketSize, eventCapacity, eventSource,
			errno);
#endif
		return (NULL);
	}

	// Fill in header fields.
	caerEventPacketHeaderSetEventType(&packet->packetHeader, SAMPLE_EVENT);
	caerEventPacketHeaderSetEventSource(&packet->packetHeader, eventSource);
	caerEventPacketHeaderSetEventSize(&packet->packetHeader, I32T(eventSize));
	caerEventPacketHeaderSetEventTSOffset(&packet->packetHeader, offsetof(struct caer_sample_event, timestamp));
	caerEventPacketHeaderSetEventTSOverflow(&packet->packetHeader, tsOverflow);
	caerEventPacketHeaderSetEventCapacity(&packet->packetHeader, eventCapacity);

	return (packet);
}

caerEarEventPacket caerEarEventPacketAllocate(int32_t eventCapacity, int16_t eventSource, int32_t tsOverflow) {
	size_t eventSize = sizeof(struct caer_ear_event);
	size_t eventPacketSize = sizeof(struct caer_ear_event_packet) + ((size_t) eventCapacity * eventSize);

	// Zero out event memory (all events invalid).
	caerEarEventPacket packet = calloc(1, eventPacketSize);
	if (packet == NULL) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Ear Event",
			"Failed to allocate %zu bytes of memory for Ear Event Packet of capacity %"
			PRIi32 " from source %" PRIi16 ". Error: %d.", eventPacketSize, eventCapacity, eventSource,
			errno);
#endif
		return (NULL);
	}

	// Fill in header fields.
	caerEventPacketHeaderSetEventType(&packet->packetHeader, EAR_EVENT);
	caerEventPacketHeaderSetEventSource(&packet->packetHeader, eventSource);
	caerEventPacketHeaderSetEventSize(&packet->packetHeader, I32T(eventSize));
	caerEventPacketHeaderSetEventTSOffset(&packet->packetHeader, offsetof(struct caer_ear_event, timestamp));
	caerEventPacketHeaderSetEventTSOverflow(&packet->packetHeader, tsOverflow);
	caerEventPacketHeaderSetEventCapacity(&packet->packetHeader, eventCapacity);

	return (packet);
}

caerConfigurationEventPacket caerConfigurationEventPacketAllocate(int32_t eventCapacity, int16_t eventSource,
	int32_t tsOverflow) {
	size_t eventSize = sizeof(struct caer_configuration_event);
	size_t eventPacketSize = sizeof(struct caer_configuration_event_packet) + ((size_t) eventCapacity * eventSize);

	// Zero out event memory (all events invalid).
	caerConfigurationEventPacket packet = calloc(1, eventPacketSize);
	if (packet == NULL) {
#if !defined(LIBCAER_LOG_NONE)
		caerLog(CAER_LOG_CRITICAL, "Configuration Event",
			"Failed to allocate %zu bytes of memory for Configuration Event Packet of capacity %"
			PRIi32 " from source %" PRIi16 ". Error: %d.", eventPacketSize, eventCapacity, eventSource,
			errno);
#endif
		return (NULL);
	}

	// Fill in header fields.
	caerEventPacketHeaderSetEventType(&packet->packetHeader, CONFIG_EVENT);
	caerEventPacketHeaderSetEventSource(&packet->packetHeader, eventSource);
	caerEventPacketHeaderSetEventSize(&packet->packetHeader, I32T(eventSize));
	caerEventPacketHeaderSetEventTSOffset(&packet->packetHeader, offsetof(struct caer_configuration_event, timestamp));
	caerEventPacketHeaderSetEventTSOverflow(&packet->packetHeader, tsOverflow);
	caerEventPacketHeaderSetEventCapacity(&packet->packetHeader, eventCapacity);

	return (packet);
}
