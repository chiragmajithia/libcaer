/**
 * @file dvs128.h
 *
 * DVS128 specific configuration defines and information structures.
 */

#ifndef LIBCAER_DEVICES_DVS128_H_
#define LIBCAER_DEVICES_DVS128_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "usb.h"
#include "../events/polarity.h"
#include "../events/special.h"

/**
 * Device type definition for iniLabs DVS128.
 */
#define CAER_DEVICE_DVS128 0

/**
 * Module address: device-side DVS configuration.
 */
#define DVS128_CONFIG_DVS  0
/**
 * Module address: device-side chip bias generator configuration.
 */
#define DVS128_CONFIG_BIAS 1

/**
 * Parameter address for module DVS128_CONFIG_DVS:
 * run the DVS chip and generate polarity event data.
 */
#define DVS128_CONFIG_DVS_RUN             0
/**
 * Parameter address for module DVS128_CONFIG_DVS:
 * reset the time-stamp counter of the device. This is a temporary
 * configuration switch and will reset itself right away.
 */
#define DVS128_CONFIG_DVS_TIMESTAMP_RESET 1
/**
 * Parameter address for module DVS128_CONFIG_DVS:
 * reset the whole DVS pixel array. This is a temporary
 * configuration switch and will reset itself right away.
 */
#define DVS128_CONFIG_DVS_ARRAY_RESET     2

#define DVS128_CONFIG_BIAS_CAS     0
#define DVS128_CONFIG_BIAS_INJGND  1
#define DVS128_CONFIG_BIAS_REQPD   2
#define DVS128_CONFIG_BIAS_PUX     3
#define DVS128_CONFIG_BIAS_DIFFOFF 4
#define DVS128_CONFIG_BIAS_REQ     5
#define DVS128_CONFIG_BIAS_REFR    6
#define DVS128_CONFIG_BIAS_PUY     7
#define DVS128_CONFIG_BIAS_DIFFON  8
#define DVS128_CONFIG_BIAS_DIFF    9
#define DVS128_CONFIG_BIAS_FOLL    10
#define DVS128_CONFIG_BIAS_PR      11

/**
 * DVS128 device-related information.
 */
struct caer_dvs128_info {
	// Unique device identifier. Also 'source' for events.
	uint16_t deviceID;
	// Device information string, for logging purposes.
	char *deviceString;
	// Logic (FPGA/CPLD) version.
	uint16_t logicVersion;
	// Whether the device is a time-stamp master or slave.
	bool deviceIsMaster;
	// DVS X axis resolution.
	uint16_t dvsSizeX;
	// DVS Y axis resolution.
	uint16_t dvsSizeY;
};

/**
 * Return basic information on the device, such as its ID, its
 * resolution, the logic version, and so on. See the 'struct
 * caer_dvs128_info' documentation for more details.
 *
 * @param handle a valid device handle.
 *
 * @return a copy of the device information structure if successful,
 *         an empty structure (all zeros) on failure.
 */
struct caer_dvs128_info caerDVS128InfoGet(caerDeviceHandle handle);

#ifdef __cplusplus
}
#endif

#endif /* LIBCAER_DEVICES_DVS128_H_ */
