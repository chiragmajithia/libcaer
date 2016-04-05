/**
 * @file frame_utils_opencv.h
 *
 * Functions for frame enhancement and demosaicing, using
 * the popular OpenCV image processing library.
 */

#ifndef LIBCAER_FRAME_UTILS_OPENCV_H_
#define LIBCAER_FRAME_UTILS_OPENCV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "events/frame.h"

enum caer_frame_utils_opencv_demosaic {
	NORMAL, VARIABLE_NUMBER_OF_GRADIENTS, EDGE_AWARE,
};

enum caer_frame_utils_opencv_auto_contrast {
	NORMALIZE, HISTOGRAM_EQUALIZATION, CLAHE,
};

enum caer_frame_utils_opencv_white_balance {
	SIMPLE, GRAYWORLD,
};

caerFrameEventPacket caerFrameUtilsOpenCVDemosaic(caerFrameEventPacket framePacket,
	enum caer_frame_utils_opencv_demosaic demosaicType);
void caerFrameUtilsOpenCVAutoContrastBrigthness(caerFrameEventPacket framePacket,
	enum caer_frame_utils_opencv_auto_contrast autoContrastType);
void caerFrameUtilsOpenCVWhiteBalance(caerFrameEventPacket framePacket,
	enum caer_frame_utils_opencv_white_balance balanceType);

#ifdef __cplusplus
}
#endif

#endif /* LIBCAER_FRAME_UTILS_OPENCV_H_ */
