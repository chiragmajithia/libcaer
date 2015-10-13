#ifndef LIBCAER_SRC_DAVIS_COMMON_H_
#define LIBCAER_SRC_DAVIS_COMMON_H_

#include "devices/davis.h"
#include "ringbuffer/ringbuffer.h"
#include <stdatomic.h>
#include <libusb-1.0/libusb.h>

#ifdef HAVE_PTHREADS
	#include "c11threads_posix.h"
#endif

#define APS_READOUT_TYPES_NUM 2
#define APS_READOUT_RESET  0
#define APS_READOUT_SIGNAL 1

#define APS_DEBUG_FRAME 0
// Use 1 for reset frame only, 2 for signal frame only

#define APS_ADC_DEPTH 10

#define IMU6_COUNT 15
#define IMU9_COUNT 21

#define DAVIS_EVENT_TYPES 4

#define DAVIS_DATA_ENDPOINT 0x82

#define VENDOR_REQUEST_FPGA_CONFIG 0xBF

struct davis_state {
	// Data Acquisition Thread -> Mainloop Exchange
	RingBuffer dataExchangeBuffer;
	atomic_uint_fast32_t dataExchangeBufferSize; // Only takes effect on DataStart() calls!
	atomic_bool dataExchangeBlocking;
	atomic_bool dataExchangeStartProducers;
	atomic_bool dataExchangeStopProducers;
	void (*dataNotifyIncrease)(void *ptr);
	void (*dataNotifyDecrease)(void *ptr);
	void *dataNotifyUserPtr;
	void (*dataShutdownNotify)(void *ptr);
	void *dataShutdownUserPtr;
	// USB Device State
	libusb_context *deviceContext;
	libusb_device_handle *deviceHandle;
	// USB Transfer Settings
	atomic_uint_fast32_t usbBufferNumber;
	atomic_uint_fast32_t usbBufferSize;
	// Data Acquisition Thread
	thrd_t dataAcquisitionThread;
	atomic_bool dataAcquisitionThreadRun;
	atomic_uint_fast32_t dataAcquisitionThreadConfigUpdate;
	struct libusb_transfer **dataTransfers;
	size_t dataTransfersLength;
	size_t activeDataTransfers;
	// Timestamp fields
	int32_t wrapOverflow;
	int32_t wrapAdd;
	int32_t lastTimestamp;
	int32_t currentTimestamp;
	// DVS specific fields
	int32_t dvsTimestamp;
	uint16_t dvsLastY;
	bool dvsGotY;
	uint16_t dvsSizeX;
	uint16_t dvsSizeY;
	bool dvsInvertXY;
	// APS specific fields
	uint16_t apsSizeX;
	uint16_t apsSizeY;
	bool apsInvertXY;
	bool apsFlipX;
	bool apsFlipY;
	bool apsIgnoreEvents;
	bool apsGlobalShutter;
	bool apsResetRead;
	bool apsRGBPixelOffsetDirection; // 0 is increasing, 1 is decreasing.
	int16_t apsRGBPixelOffset;
	uint16_t apsCurrentReadoutType;
	uint16_t apsCountX[APS_READOUT_TYPES_NUM];
	uint16_t apsCountY[APS_READOUT_TYPES_NUM];
	uint16_t *apsCurrentResetFrame;
	uint16_t apsROI0SizeX;
	uint16_t apsROI0SizeY;
	uint16_t apsROI0PositionX;
	uint16_t apsROI0PositionY;
	uint16_t apsROI1SizeX;
	uint16_t apsROI1SizeY;
	uint16_t apsROI1PositionX;
	uint16_t apsROI1PositionY;
	uint16_t apsROI2SizeX;
	uint16_t apsROI2SizeY;
	uint16_t apsROI2PositionX;
	uint16_t apsROI2PositionY;
	uint16_t apsROI3SizeX;
	uint16_t apsROI3SizeY;
	uint16_t apsROI3PositionX;
	uint16_t apsROI3PositionY;
	// IMU specific fields
	bool imuIgnoreEvents;
	uint8_t imuCount;
	uint8_t imuTmpData;
	float imuAccelScale;
	float imuGyroScale;
	// Packet Container state
	caerEventPacketContainer currentPacketContainer;
	atomic_int_fast32_t maxPacketContainerSize;
	atomic_int_fast32_t maxPacketContainerInterval;
	// Polarity Packet state
	caerPolarityEventPacket currentPolarityPacket;
	int32_t currentPolarityPacketPosition;
	atomic_int_fast32_t maxPolarityPacketSize;
	atomic_int_fast32_t maxPolarityPacketInterval;
	// Frame Packet state
	caerFrameEventPacket currentFramePacket;
	int32_t currentFramePacketPosition;
	atomic_int_fast32_t maxFramePacketSize;
	atomic_int_fast32_t maxFramePacketInterval;
	// IMU6 Packet state
	caerIMU6EventPacket currentIMU6Packet;
	int32_t currentIMU6PacketPosition;
	atomic_int_fast32_t maxIMU6PacketSize;
	atomic_int_fast32_t maxIMU6PacketInterval;
	// Special Packet state
	caerSpecialEventPacket currentSpecialPacket;
	int32_t currentSpecialPacketPosition;
	atomic_int_fast32_t maxSpecialPacketSize;
	atomic_int_fast32_t maxSpecialPacketInterval;
	// Current composite events, for later copy, to not loose them on commits.
	struct caer_frame_event currentFrameEvent0;
	struct caer_frame_event currentFrameEvent1;
	struct caer_frame_event currentFrameEvent2;
	struct caer_frame_event currentFrameEvent3;
	struct caer_imu6_event currentIMU6Event;
};

typedef struct davis_state *davisState;

struct davis_handle {
	uint16_t deviceType;
	// Information fields
	struct caer_davis_info info;
	// State for data management, common to all DAVIS.
	struct davis_state state;
};

typedef struct davis_handle *davisHandle;

bool davisCommonOpen(davisHandle handle, uint16_t VID, uint16_t PID, uint8_t DID_TYPE, const char *deviceName,
	uint16_t deviceID, uint8_t busNumberRestrict, uint8_t devAddressRestrict, const char *serialNumberRestrict,
	uint16_t requiredLogicRevision, uint16_t requiredFirmwareVersion);
bool davisCommonClose(davisHandle handle);

bool davisCommonSendDefaultFPGAConfig(caerDeviceHandle cdh,
	bool (*configSet)(caerDeviceHandle cdh, int8_t modAddr, uint8_t paramAddr, uint32_t param));
bool davisCommonSendDefaultChipConfig(caerDeviceHandle cdh,
	bool (*configSet)(caerDeviceHandle cdh, int8_t modAddr, uint8_t paramAddr, uint32_t param));

bool davisCommonConfigSet(davisHandle handle, int8_t modAddr, uint8_t paramAddr, uint32_t param);
bool davisCommonConfigGet(davisHandle handle, int8_t modAddr, uint8_t paramAddr, uint32_t *param);

bool davisCommonDataStart(caerDeviceHandle handle, void (*dataNotifyIncrease)(void *ptr),
	void (*dataNotifyDecrease)(void *ptr), void *dataNotifyUserPtr, void (*dataShutdownNotify)(void *ptr),
	void *dataShutdownUserPtr);
bool davisCommonDataStop(caerDeviceHandle handle);
caerEventPacketContainer davisCommonDataGet(caerDeviceHandle handle);

#endif /* LIBCAER_SRC_DAVIS_COMMON_H_ */
