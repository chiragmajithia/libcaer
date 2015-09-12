#ifndef LIBCAER_DEVICES_USB_H_
#define LIBCAER_DEVICES_USB_H_

#include "libcaer.h"
#include "events/packetContainer.h"

typedef struct caer_device_handle *caerDeviceHandle;

#define HOST_CONFIG_USB -1
#define HOST_CONFIG_DATAEXCHANGE -2

#define HOST_CONFIG_USB_BUFFER_NUMBER 0
#define HOST_CONFIG_USB_BUFFER_SIZE   1

#define HOST_CONFIG_DATAEXCHANGE_BUFFER_SIZE 0
#define HOST_CONFIG_DATAEXCHANGE_BLOCKING    1

caerDeviceHandle caerDeviceOpen(uint16_t deviceType, uint8_t busNumberRestrict, uint8_t devAddressRestrict,
	const char *serialNumberRestrict);
bool caerDeviceClose(caerDeviceHandle *handle);

bool caerDeviceSendDefaultConfig(caerDeviceHandle handle);
// Negative addresses are used for host-side configuration.
// Positive addresses (including zero) are used for device-side configuration.
bool caerDeviceConfigSet(caerDeviceHandle handle, int8_t modAddr, uint8_t paramAddr, uint32_t param);
bool caerDeviceConfigGet(caerDeviceHandle handle, int8_t modAddr, uint8_t paramAddr, uint32_t *param);

bool caerDeviceDataStart(caerDeviceHandle handle, void (*dataNotifyIncrease)(void *ptr),
	void (*dataNotifyDecrease)(void *ptr), void *dataNotifyUserPtr);
bool caerDeviceDataStop(caerDeviceHandle handle);
caerEventPacketContainer caerDeviceDataGet(caerDeviceHandle handle);

#endif /* LIBCAER_DEVICES_USB_H_ */
