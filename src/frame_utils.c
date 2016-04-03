#include "frame_utils.h"

static void frameUtilsDemosaicFrame(caerFrameEvent colorFrame, caerFrameEvent monoFrame);

caerFrameEventPacket caerFrameUtilsDemosaic(caerFrameEventPacket framePacket) {
	int32_t countValid = 0;
	int32_t maxLengthX = 0;
	int32_t maxLengthY = 0;

	// This only works on valid frames coming from a camera: only one color channel,
	// but with color filter information defined.
	CAER_FRAME_ITERATOR_ALL_START(framePacket)
		if (caerFrameEventIsValid(caerFrameIteratorElement)
			&& caerFrameEventGetChannelNumber(caerFrameIteratorElement) == GRAYSCALE
			&& caerFrameEventGetColorFilter(caerFrameIteratorElement) != MONO) {
			countValid++;

			if (caerFrameEventGetLengthX(caerFrameIteratorElement) > maxLengthX) {
				maxLengthX = caerFrameEventGetLengthX(caerFrameIteratorElement);
			}

			if (caerFrameEventGetLengthY(caerFrameIteratorElement) > maxLengthY) {
				maxLengthY = caerFrameEventGetLengthY(caerFrameIteratorElement);
			}
		}
	CAER_FRAME_ITERATOR_ALL_END

	// Allocate new frame with RGB channels to hold resulting color image.
	caerFrameEventPacket colorFramePacket = caerFrameEventPacketAllocate(countValid,
		caerEventPacketHeaderGetEventSource(&framePacket->packetHeader),
		caerEventPacketHeaderGetEventTSOverflow(&framePacket->packetHeader), maxLengthX, maxLengthY, RGB);

	int32_t colorIndex = 0;

	// Now that we have a valid new color frame packet, we can convert the frames one by one.
	CAER_FRAME_ITERATOR_ALL_START(framePacket)
		if (caerFrameEventIsValid(caerFrameIteratorElement)
			&& caerFrameEventGetChannelNumber(caerFrameIteratorElement) == GRAYSCALE
			&& caerFrameEventGetColorFilter(caerFrameIteratorElement) != MONO) {
			// If all conditions are met, copy from framePacket's mono frame to colorFramePacket's RGB frame.
			caerFrameEvent colorFrame = caerFrameEventPacketGetEvent(colorFramePacket, colorIndex++);

			// First copy all the metadata.
			caerFrameEventSetColorFilter(colorFrame, caerFrameEventGetColorFilter(caerFrameIteratorElement));
			caerFrameEventSetLengthXLengthYChannelNumber(colorFrame, caerFrameEventGetLengthX(caerFrameIteratorElement),
				caerFrameEventGetLengthY(caerFrameIteratorElement), RGB, colorFramePacket);
			caerFrameEventSetPositionX(colorFrame, caerFrameEventGetPositionX(caerFrameIteratorElement));
			caerFrameEventSetPositionY(colorFrame, caerFrameEventGetPositionY(caerFrameIteratorElement));
			caerFrameEventSetROIIdentifier(colorFrame, caerFrameEventGetROIIdentifier(caerFrameIteratorElement));
			caerFrameEventSetTSStartOfFrame(colorFrame, caerFrameEventGetTSStartOfFrame(caerFrameIteratorElement));
			caerFrameEventSetTSEndOfFrame(colorFrame, caerFrameEventGetTSEndOfFrame(caerFrameIteratorElement));
			caerFrameEventSetTSStartOfExposure(colorFrame,
				caerFrameEventGetTSStartOfExposure(caerFrameIteratorElement));
			caerFrameEventSetTSEndOfExposure(colorFrame, caerFrameEventGetTSEndOfExposure(caerFrameIteratorElement));

			// Then the actual pixels.
			frameUtilsDemosaicFrame(colorFrame, caerFrameIteratorElement);

			// Finally validate the new frame.
			caerFrameEventValidate(colorFrame, colorFramePacket);
		}
	CAER_FRAME_ITERATOR_ALL_END

	return (colorFramePacket);
}

static void frameUtilsDemosaicFrame(caerFrameEvent colorFrame, caerFrameEvent monoFrame) {

}

void caerFrameUtilsAutoContrastBrigthness(caerFrameEventPacket framePacket) {
	// O(x, y) = alpha * I(x, y) + beta, where alpha maximizes the range
	// (contrast) and beta shifts it so lowest is zero (brightness).

}
