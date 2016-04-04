#include "frame_utils_opencv.h"

#include <opencv2/core.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgproc.hpp>

using namespace cv;
using namespace std;

extern "C" {

static void frameUtilsOpenCVDemosaicFrame(caerFrameEvent colorFrame, caerFrameEvent monoFrame);

static void frameUtilsOpenCVDemosaicFrame(caerFrameEvent colorFrame, caerFrameEvent monoFrame) {

}

caerFrameEventPacket caerFrameUtilsOpenCVDemosaic(caerFrameEventPacket framePacket) {
	int32_t countValid = 0;
	int32_t maxLengthX = 0;
	int32_t maxLengthY = 0;

	// This only works on valid frames coming from a camera: only one color channel,
	// but with color filter information defined.
	CAER_FRAME_ITERATOR_VALID_START(framePacket)
		if (caerFrameEventGetChannelNumber(caerFrameIteratorElement) == GRAYSCALE
			&& caerFrameEventGetColorFilter(caerFrameIteratorElement) != MONO) {
			countValid++;

			if (caerFrameEventGetLengthX(caerFrameIteratorElement) > maxLengthX) {
				maxLengthX = caerFrameEventGetLengthX(caerFrameIteratorElement);
			}

			if (caerFrameEventGetLengthY(caerFrameIteratorElement) > maxLengthY) {
				maxLengthY = caerFrameEventGetLengthY(caerFrameIteratorElement);
			}
		}
	CAER_FRAME_ITERATOR_VALID_END

	// Allocate new frame with RGB channels to hold resulting color image.
	caerFrameEventPacket colorFramePacket = caerFrameEventPacketAllocate(countValid,
		caerEventPacketHeaderGetEventSource(&framePacket->packetHeader),
		caerEventPacketHeaderGetEventTSOverflow(&framePacket->packetHeader), maxLengthX, maxLengthY, RGB);

	int32_t colorIndex = 0;

	// Now that we have a valid new color frame packet, we can convert the frames one by one.
	CAER_FRAME_ITERATOR_VALID_START(framePacket)
		if (caerFrameEventGetChannelNumber(caerFrameIteratorElement) == GRAYSCALE
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
			frameUtilsOpenCVDemosaicFrame(colorFrame, caerFrameIteratorElement);

			// Finally validate the new frame.
			caerFrameEventValidate(colorFrame, colorFramePacket);
		}
	CAER_FRAME_ITERATOR_VALID_END

	return (colorFramePacket);
}

void caerFrameUtilsOpenCVAutoContrastBrigthness(caerFrameEventPacket framePacket) {

}

}
