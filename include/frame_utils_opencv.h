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

caerFrameEventPacket caerFrameUtilsOpenCVDemosaic(caerFrameEventPacket framePacket);
void caerFrameUtilsOpenCVAutoContrastBrigthness(caerFrameEventPacket framePacket);

#ifdef __cplusplus
}
#endif

#endif /* LIBCAER_FRAME_UTILS_OPENCV_H_ */
