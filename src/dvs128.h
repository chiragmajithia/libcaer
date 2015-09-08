#ifndef LIBCAER_SRC_DVS128_H_
#define LIBCAER_SRC_DVS128_H_

#include "devices/dvs128.h"
#include "ringbuffer/ringbuffer.h"
#include <pthread.h>
#include <unistd.h>
#include <libusb.h>
#include <stdatomic.h>

#define DVS128_VID 0x152A
#define DVS128_PID 0x8400
#define DVS128_DID_TYPE 0x00

#define DVS128_ARRAY_SIZE_X 128
#define DVS128_ARRAY_SIZE_Y 128

#define DATA_ENDPOINT 0x86

#define VENDOR_REQUEST_START_TRANSFER 0xB3
#define VENDOR_REQUEST_STOP_TRANSFER 0xB4
#define VENDOR_REQUEST_SEND_BIASES 0xB8
#define VENDOR_REQUEST_RESET_TS 0xBB
#define VENDOR_REQUEST_RESET_ARRAY 0xBD

struct dvs128_state {
	// Data Acquisition Thread -> Mainloop Exchange
	RingBuffer dataExchangeBuffer;
	atomic_ulong dataNotify;
	// USB Device State
	libusb_context *deviceContext;
	libusb_device_handle *deviceHandle;
	// Data Acquisition Thread
	pthread_t dataAcquisitionThread;
	struct libusb_transfer **dataTransfers;
	size_t dataTransfersLength;
	size_t activeDataTransfers;
	// Timestamp fields
	uint32_t wrapOverflow;
	uint32_t wrapAdd;
	uint32_t lastTimestamp;
	uint32_t currentTimestamp;
	// Packet Container state
	caerEventPacketContainer currentPacketContainer;
	uint32_t maxPacketContainerSize;
	uint32_t maxPacketContainerInterval;
	// Polarity Packet State
	caerPolarityEventPacket currentPolarityPacket;
	uint32_t currentPolarityPacketPosition;
	uint32_t maxPolarityPacketSize;
	uint32_t maxPolarityPacketInterval;
	// Special Packet State
	caerSpecialEventPacket currentSpecialPacket;
	uint32_t currentSpecialPacketPosition;
	uint32_t maxSpecialPacketSize;
	uint32_t maxSpecialPacketInterval;
	// Data Transfer State
	uint32_t usbBufferNumber;
	uint32_t usbBufferSize;
	uint32_t dataExchangeBufferSize;
	bool dataExchangeBlockOnEmpty;
};

typedef struct dvs128_state *dvs128State;

struct dvs128_handle {
	uint16_t deviceType;
	// Information fields
	struct caer_dvs128_info info;
	// State for data management.
	struct dvs128_state state;
};

typedef struct dvs128_handle *dvs128Handle;

caerDeviceHandle dvs128Open(uint8_t busNumberRestrict, uint8_t devAddressRestrict, const char *serialNumberRestrict);
bool dvs128Close(caerDeviceHandle handle);

bool dvs128SendDefaultConfig(caerDeviceHandle handle);
// Negative addresses are used for host-side configuration.
// Positive addresses (including zero) are used for device-side configuration.
bool dvs128ConfigSet(caerDeviceHandle handle, int8_t modAddr, uint8_t paramAddr, uint32_t param);
bool dvs128ConfigGet(caerDeviceHandle handle, int8_t modAddr, uint8_t paramAddr, uint32_t *param);

bool dvs128DataStart(caerDeviceHandle handle);
bool dvs128DataStop(caerDeviceHandle handle);
caerEventPacketContainer dvs128DataGet(caerDeviceHandle handle);

#endif /* LIBCAER_SRC_DVS128_H_ */
