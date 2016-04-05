#include "frame_utils_opencv.h"

#include <opencv2/core.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgproc.hpp>

using namespace cv;
using namespace std;

static void frameUtilsOpenCVDemosaicFrame(caerFrameEvent colorFrame, caerFrameEvent monoFrame,
	enum caer_frame_utils_opencv_demosaic demosaicType);

static void frameUtilsOpenCVDemosaicFrame(caerFrameEvent colorFrame, caerFrameEvent monoFrame,
	enum caer_frame_utils_opencv_demosaic demosaicType) {
	// Initialize OpenCV Mat based on caerFrameEvent data directly (no image copy).
	Size frameSize(caerFrameEventGetLengthX(monoFrame), caerFrameEventGetLengthY(monoFrame));
	Mat monoMat(frameSize, CV_16UC(caerFrameEventGetChannelNumber(monoFrame)),
		caerFrameEventGetPixelArrayUnsafe(monoFrame));
	Mat colorMat(frameSize, CV_16UC(caerFrameEventGetChannelNumber(colorFrame)),
		caerFrameEventGetPixelArrayUnsafe(colorFrame));

	// Select correct type code for OpenCV demosaic algorithm.
	int code = 0;

	switch (demosaicType) {
		case NORMAL:
			switch (caerFrameEventGetColorFilter(monoFrame)) {
				case RGBG:
					code = COLOR_BayerBG2RGB;
					break;

				case GRGB:
					code = COLOR_BayerGB2RGB;
					break;

				case GBGR:
					code = COLOR_BayerGR2RGB;
					break;

				case BGRG:
					code = COLOR_BayerRG2RGB;
					break;

				default:
					// Impossible, other color filters get filtered out above.
					break;
			}
			break;

		case VARIABLE_NUMBER_OF_GRADIENTS:
			switch (caerFrameEventGetColorFilter(monoFrame)) {
				case RGBG:
					code = COLOR_BayerBG2RGB_VNG;
					break;

				case GRGB:
					code = COLOR_BayerGB2RGB_VNG;
					break;

				case GBGR:
					code = COLOR_BayerGR2RGB_VNG;
					break;

				case BGRG:
					code = COLOR_BayerRG2RGB_VNG;
					break;

				default:
					// Impossible, other color filters get filtered out above.
					break;
			}
			break;

		case EDGE_AWARE:
			switch (caerFrameEventGetColorFilter(monoFrame)) {
				case RGBG:
					code = COLOR_BayerBG2RGB_EA;
					break;

				case GRGB:
					code = COLOR_BayerGB2RGB_EA;
					break;

				case GBGR:
					code = COLOR_BayerGR2RGB_EA;
					break;

				case BGRG:
					code = COLOR_BayerRG2RGB_EA;
					break;

				default:
					// Impossible, other color filters get filtered out above.
					break;
			}
			break;
	}

	// Convert Bayer pattern to RGB image.
	cvtColor(monoMat, colorMat, code);
}

caerFrameEventPacket caerFrameUtilsOpenCVDemosaic(caerFrameEventPacket framePacket,
	enum caer_frame_utils_opencv_demosaic demosaicType) {
	if (framePacket == NULL) {
		return (NULL);
	}

	int32_t countValid = 0;
	int32_t maxLengthX = 0;
	int32_t maxLengthY = 0;

	// This only works on valid frames coming from a camera: only one color channel,
	// but with color filter information defined.
	CAER_FRAME_ITERATOR_VALID_START(framePacket)
		if (caerFrameEventGetChannelNumber(caerFrameIteratorElement) == GRAYSCALE
			&& caerFrameEventGetColorFilter(caerFrameIteratorElement) != MONO) {
			if (caerFrameEventGetColorFilter(caerFrameIteratorElement) == RGBG
				|| caerFrameEventGetColorFilter(caerFrameIteratorElement) == GRGB
				|| caerFrameEventGetColorFilter(caerFrameIteratorElement) == GBGR
				|| caerFrameEventGetColorFilter(caerFrameIteratorElement) == BGRG) {
				countValid++;

				if (caerFrameEventGetLengthX(caerFrameIteratorElement) > maxLengthX) {
					maxLengthX = caerFrameEventGetLengthX(caerFrameIteratorElement);
				}

				if (caerFrameEventGetLengthY(caerFrameIteratorElement) > maxLengthY) {
					maxLengthY = caerFrameEventGetLengthY(caerFrameIteratorElement);
				}
			}
			else {
				caerLog(CAER_LOG_WARNING, "caerFrameUtilsOpenCVDemosaic()",
					"OpenCV demosaicing doesn't support the RGBW color filter, only RGBG. Please use caerFrameUtilsDemosaic() instead.");
			}
		}CAER_FRAME_ITERATOR_VALID_END

	// Allocate new frame with RGB channels to hold resulting color image.
	caerFrameEventPacket colorFramePacket = caerFrameEventPacketAllocate(countValid,
		caerEventPacketHeaderGetEventSource(&framePacket->packetHeader),
		caerEventPacketHeaderGetEventTSOverflow(&framePacket->packetHeader), maxLengthX, maxLengthY, RGB);
	if (colorFramePacket == NULL) {
		return (NULL);
	}

	int32_t colorIndex = 0;

	// Now that we have a valid new color frame packet, we can convert the frames one by one.
	CAER_FRAME_ITERATOR_VALID_START(framePacket)
		if (caerFrameEventGetChannelNumber(caerFrameIteratorElement) == GRAYSCALE
			&& caerFrameEventGetColorFilter(caerFrameIteratorElement) != MONO) {
			if (caerFrameEventGetColorFilter(caerFrameIteratorElement) == RGBG
				|| caerFrameEventGetColorFilter(caerFrameIteratorElement) == GRGB
				|| caerFrameEventGetColorFilter(caerFrameIteratorElement) == GBGR
				|| caerFrameEventGetColorFilter(caerFrameIteratorElement) == BGRG) {
				// If all conditions are met, copy from framePacket's mono frame to colorFramePacket's RGB frame.
				caerFrameEvent colorFrame = caerFrameEventPacketGetEvent(colorFramePacket, colorIndex++);

				// First copy all the metadata.
				caerFrameEventSetColorFilter(colorFrame, caerFrameEventGetColorFilter(caerFrameIteratorElement));
				caerFrameEventSetLengthXLengthYChannelNumber(colorFrame,
					caerFrameEventGetLengthX(caerFrameIteratorElement),
					caerFrameEventGetLengthY(caerFrameIteratorElement), RGB, colorFramePacket);
				caerFrameEventSetPositionX(colorFrame, caerFrameEventGetPositionX(caerFrameIteratorElement));
				caerFrameEventSetPositionY(colorFrame, caerFrameEventGetPositionY(caerFrameIteratorElement));
				caerFrameEventSetROIIdentifier(colorFrame, caerFrameEventGetROIIdentifier(caerFrameIteratorElement));
				caerFrameEventSetTSStartOfFrame(colorFrame, caerFrameEventGetTSStartOfFrame(caerFrameIteratorElement));
				caerFrameEventSetTSEndOfFrame(colorFrame, caerFrameEventGetTSEndOfFrame(caerFrameIteratorElement));
				caerFrameEventSetTSStartOfExposure(colorFrame,
					caerFrameEventGetTSStartOfExposure(caerFrameIteratorElement));
				caerFrameEventSetTSEndOfExposure(colorFrame,
					caerFrameEventGetTSEndOfExposure(caerFrameIteratorElement));

				// Then the actual pixels. Only supports RGBG!
				frameUtilsOpenCVDemosaicFrame(colorFrame, caerFrameIteratorElement, demosaicType);

				// Finally validate the new frame.
				caerFrameEventValidate(colorFrame, colorFramePacket);
			}
		}CAER_FRAME_ITERATOR_VALID_END

	return (colorFramePacket);
}

void caerFrameUtilsOpenCVAutoContrastBrigthness(caerFrameEventPacket framePacket,
	enum caer_frame_utils_opencv_auto_contrast autoContrastType) {

}

void caerFrameUtilsOpenCVWhiteBalance(caerFrameEventPacket framePacket,
	enum caer_frame_utils_opencv_white_balance balanceType) {

}
