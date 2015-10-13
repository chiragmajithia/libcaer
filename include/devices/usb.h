/**
 * @file usb.h
 *
 * Common functions to access, configure and exchange data with
 * supported USB devices. Also contains defines for host/USB
 * related configuration options.
 */

#ifndef LIBCAER_DEVICES_USB_H_
#define LIBCAER_DEVICES_USB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "../libcaer.h"
#include "../events/packetContainer.h"

/**
 * Reference to an open device on which to operate.
 */
typedef struct caer_device_handle *caerDeviceHandle;

/**
 * Module address: host-side USB configuration.
 */
#define CAER_HOST_CONFIG_USB -1
/**
 * Module address: host-side data exchange (ringbuffer) configuration.
 */
#define CAER_HOST_CONFIG_DATAEXCHANGE -2
/**
 * Module address: host-side event packets generation configuration.
 */
#define CAER_HOST_CONFIG_PACKETS -3

/**
 * Parameter address for module CAER_HOST_CONFIG_USB:
 * set number of buffers used by libusb for asynchronous data transfers
 * with the USB device.
 */
#define CAER_HOST_CONFIG_USB_BUFFER_NUMBER 0
/**
 * Parameter address for module CAER_HOST_CONFIG_USB:
 * set size of each buffer used by libusb for asynchronous data transfers
 * with the USB device.
 */
#define CAER_HOST_CONFIG_USB_BUFFER_SIZE   1

#define CAER_HOST_CONFIG_DATAEXCHANGE_BUFFER_SIZE     0
#define CAER_HOST_CONFIG_DATAEXCHANGE_BLOCKING        1
#define CAER_HOST_CONFIG_DATAEXCHANGE_START_PRODUCERS 2
#define CAER_HOST_CONFIG_DATAEXCHANGE_STOP_PRODUCERS  3

#define CAER_HOST_CONFIG_PACKETS_MAX_CONTAINER_SIZE     0
#define CAER_HOST_CONFIG_PACKETS_MAX_CONTAINER_INTERVAL 1
#define CAER_HOST_CONFIG_PACKETS_MAX_POLARITY_SIZE      2
#define CAER_HOST_CONFIG_PACKETS_MAX_POLARITY_INTERVAL  3
#define CAER_HOST_CONFIG_PACKETS_MAX_SPECIAL_SIZE       4
#define CAER_HOST_CONFIG_PACKETS_MAX_SPECIAL_INTERVAL   5
#define CAER_HOST_CONFIG_PACKETS_MAX_FRAME_SIZE         6
#define CAER_HOST_CONFIG_PACKETS_MAX_FRAME_INTERVAL     7
#define CAER_HOST_CONFIG_PACKETS_MAX_IMU6_SIZE          8
#define CAER_HOST_CONFIG_PACKETS_MAX_IMU6_INTERVAL      9

caerDeviceHandle caerDeviceOpen(uint16_t deviceID, uint16_t deviceType, uint8_t busNumberRestrict,
	uint8_t devAddressRestrict, const char *serialNumberRestrict);
bool caerDeviceClose(caerDeviceHandle *handle);

bool caerDeviceSendDefaultConfig(caerDeviceHandle handle);
// Negative addresses are used for host-side configuration.
// Positive addresses (including zero) are used for device-side configuration.
bool caerDeviceConfigSet(caerDeviceHandle handle, int8_t modAddr, uint8_t paramAddr, uint32_t param);
bool caerDeviceConfigGet(caerDeviceHandle handle, int8_t modAddr, uint8_t paramAddr, uint32_t *param);

bool caerDeviceDataStart(caerDeviceHandle handle, void (*dataNotifyIncrease)(void *ptr),
	void (*dataNotifyDecrease)(void *ptr), void *dataNotifyUserPtr, void (*dataShutdownNotify)(void *ptr),
	void *dataShutdownUserPtr);
bool caerDeviceDataStop(caerDeviceHandle handle);
caerEventPacketContainer caerDeviceDataGet(caerDeviceHandle handle);

#ifdef __cplusplus
}
#endif

#endif /* LIBCAER_DEVICES_USB_H_ */
