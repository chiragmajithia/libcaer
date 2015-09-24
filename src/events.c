#include "events/frame.h"

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
