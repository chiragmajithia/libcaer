#include "dvs128.h"

static libusb_device_handle *dvs128DeviceOpen(libusb_context *devContext, uint8_t busNumber, uint8_t devAddress);
static void dvs128DeviceClose(libusb_device_handle *devHandle);
static void dvs128AllocateTransfers(dvs128Handle handle, uint32_t bufferNum, uint32_t bufferSize);
static void dvs128DeallocateTransfers(dvs128Handle handle);
static void LIBUSB_CALL dvs128LibUsbCallback(struct libusb_transfer *transfer);
static void dvs128EventTranslator(dvs128Handle handle, uint8_t *buffer, size_t bytesSent);
static void dvs128SendBiases(dvs128State state);
static void *dvs128DataAcquisitionThread(void *inPtr);
static void dvs128DataAcquisitionThreadConfig(dvs128Handle handle);

static inline void checkMonotonicTimestamp(dvs128Handle handle) {
	if (handle->state.currentTimestamp < handle->state.lastTimestamp) {
		caerLog(LOG_ALERT, handle->info.deviceString,
			"Timestamps: non monotonic timestamp detected: lastTimestamp=%" PRIi32 ", currentTimestamp=%" PRIi32 ", difference=%" PRIi32 ".",
			handle->state.lastTimestamp, handle->state.currentTimestamp,
			(handle->state.lastTimestamp - handle->state.currentTimestamp));
	}
}

static inline void freeAllDataMemory(dvs128State state) {
	if (state->dataExchangeBuffer != NULL) {
		ringBufferFree(state->dataExchangeBuffer);
		state->dataExchangeBuffer = NULL;
	}

	// Since the current event packets aren't necessarily
	// already assigned to the current packet container, we
	// free them separately from it.
	if (state->currentPolarityPacket != NULL) {
		caerEventPacketFree(&state->currentPolarityPacket->packetHeader);
		state->currentPolarityPacket = NULL;

		if (state->currentPacketContainer != NULL) {
			caerEventPacketContainerSetEventPacket(state->currentPacketContainer, POLARITY_EVENT, NULL);
		}
	}

	if (state->currentSpecialPacket != NULL) {
		caerEventPacketFree(&state->currentSpecialPacket->packetHeader);
		state->currentSpecialPacket = NULL;

		if (state->currentPacketContainer != NULL) {
			caerEventPacketContainerSetEventPacket(state->currentPacketContainer, SPECIAL_EVENT, NULL);
		}
	}

	if (state->currentPacketContainer != NULL) {
		caerEventPacketContainerFree(state->currentPacketContainer);
		state->currentPacketContainer = NULL;
	}
}

caerDeviceHandle dvs128Open(uint16_t deviceID, uint8_t busNumberRestrict, uint8_t devAddressRestrict,
	const char *serialNumberRestrict) {
	caerLog(LOG_DEBUG, __func__, "Initializing " DEVICE_NAME ".");

	dvs128Handle handle = calloc(1, sizeof(*handle));
	if (handle == NULL) {
		// Failed to allocate memory for device handle!
		caerLog(LOG_CRITICAL, __func__, "Failed to allocate memory for device handle.");
		return (NULL);
	}

	dvs128State state = &handle->state;

	// Initialize state variables to default values (if not zero, taken care of by calloc above).
	atomic_store(&state->dataExchangeBufferSize, 64);
	atomic_store(&state->dataExchangeBlocking, false);
	atomic_store(&state->usbBufferNumber, 8);
	atomic_store(&state->usbBufferSize, 4096);

	// Packet settings (size (in events) and time interval (in µs)).
	atomic_store(&state->maxPacketContainerSize, 4096 + 128);
	atomic_store(&state->maxPacketContainerInterval, 5000);
	atomic_store(&state->maxPolarityPacketSize, 4096);
	atomic_store(&state->maxPolarityPacketInterval, 5000);
	atomic_store(&state->maxSpecialPacketSize, 128);
	atomic_store(&state->maxSpecialPacketInterval, 1000);

	// Search for device and open it.
	// Initialize libusb using a separate context for each device.
	// This is to correctly support one thread per device.
	if ((errno = libusb_init(&state->deviceContext)) != LIBUSB_SUCCESS) {
		free(handle);

		caerLog(LOG_CRITICAL, __func__, "Failed to initialize libusb context. Error: %d.", errno);
		return (NULL);
	}

	// Try to open a DVS128 device on a specific USB port.
	state->deviceHandle = dvs128DeviceOpen(state->deviceContext, busNumberRestrict, devAddressRestrict);
	if (state->deviceHandle == NULL) {
		libusb_exit(state->deviceContext);
		free(handle);

		caerLog(LOG_CRITICAL, __func__, "Failed to open " DEVICE_NAME " device.");
		return (NULL);
	}

	// At this point we can get some more precise data on the device and update
	// the logging string to reflect that and be more informative.
	uint8_t busNumber = libusb_get_bus_number(libusb_get_device(state->deviceHandle));
	uint8_t devAddress = libusb_get_device_address(libusb_get_device(state->deviceHandle));

	char serialNumber[8 + 1];
	libusb_get_string_descriptor_ascii(state->deviceHandle, 3, (unsigned char *) serialNumber, 8 + 1);
	serialNumber[8] = '\0'; // Ensure NUL termination.

	size_t fullLogStringLength = (size_t) snprintf(NULL, 0, "%s ID-%" PRIu16 " SN-%s [%" PRIu8 ":%" PRIu8 "]",
	DEVICE_NAME, deviceID, serialNumber, busNumber, devAddress);

	char *fullLogString = malloc(fullLogStringLength + 1);
	if (fullLogString == NULL) {
		libusb_close(state->deviceHandle);
		libusb_exit(state->deviceContext);
		free(handle);

		caerLog(LOG_CRITICAL, __func__, "Unable to allocate memory for device info string.");
		return (NULL);
	}

	snprintf(fullLogString, fullLogStringLength + 1, "%s ID-%" PRIu16 " SN-%s [%" PRIu8 ":%" PRIu8 "]", DEVICE_NAME,
		deviceID, serialNumber, busNumber, devAddress);

	// Now check if the Serial Number matches.
	if (!str_equals(serialNumberRestrict, "") && !str_equals(serialNumberRestrict, serialNumber)) {
		libusb_close(state->deviceHandle);
		libusb_exit(state->deviceContext);
		free(handle);

		caerLog(LOG_CRITICAL, fullLogString, "Device Serial Number doesn't match.");
		free(fullLogString);

		return (NULL);
	}

	// Populate info variables based on data from device.
	handle->info.deviceID = deviceID;
	handle->info.deviceString = fullLogString;
	handle->info.logicVersion = 1;
	handle->info.deviceIsMaster = true; // TODO: master/slave support.
	handle->info.dvsSizeX = DVS_ARRAY_SIZE_X;
	handle->info.dvsSizeY = DVS_ARRAY_SIZE_Y;

	// Verify device logic version.
	if (handle->info.logicVersion < REQUIRED_LOGIC_REVISION) {
		libusb_close(state->deviceHandle);
		libusb_exit(state->deviceContext);
		free(handle);

		// Logic too old, notify and quit.
		caerLog(LOG_CRITICAL, fullLogString,
			"Device logic revision too old. You have revision %u; but at least revision %u is required. Please updated by following the Flashy upgrade documentation at 'https://goo.gl/TGM0w1'.",
			handle->info.logicVersion, REQUIRED_LOGIC_REVISION);
		free(fullLogString);

		return (NULL);
	}

	caerLog(LOG_DEBUG, fullLogString, "Initialized device successfully with USB Bus=%" PRIu8 ":Addr=%" PRIu8 ".",
		busNumber, devAddress);

	return ((caerDeviceHandle) handle);
}

bool dvs128Close(caerDeviceHandle cdh) {
	dvs128Handle handle = (dvs128Handle) cdh;
	dvs128State state = &handle->state;

	// Finally, close the device fully.
	dvs128DeviceClose(state->deviceHandle);

	// Destroy libusb context.
	libusb_exit(state->deviceContext);

	caerLog(LOG_DEBUG, handle->info.deviceString, "Shutdown successful.");

	// Free memory.
	free(handle->info.deviceString);
	free(handle);

	return (true);
}

bool dvs128SendDefaultConfig(caerDeviceHandle cdh) {
	dvs128Handle handle = (dvs128Handle) cdh;
	dvs128State state = &handle->state;

	// Set all biases to default value.
	integerToByteArray(1992, state->biases[DVS128_CONFIG_BIAS_CAS], BIAS_LENGTH);
	integerToByteArray(1108364, state->biases[DVS128_CONFIG_BIAS_INJGND], BIAS_LENGTH);
	integerToByteArray(16777215, state->biases[DVS128_CONFIG_BIAS_REQPD], BIAS_LENGTH);
	integerToByteArray(8159221, state->biases[DVS128_CONFIG_BIAS_PUX], BIAS_LENGTH);
	integerToByteArray(132, state->biases[DVS128_CONFIG_BIAS_DIFFOFF], BIAS_LENGTH);
	integerToByteArray(309590, state->biases[DVS128_CONFIG_BIAS_REQ], BIAS_LENGTH);
	integerToByteArray(969, state->biases[DVS128_CONFIG_BIAS_REFR], BIAS_LENGTH);
	integerToByteArray(16777215, state->biases[DVS128_CONFIG_BIAS_PUY], BIAS_LENGTH);
	integerToByteArray(209996, state->biases[DVS128_CONFIG_BIAS_DIFFON], BIAS_LENGTH);
	integerToByteArray(13125, state->biases[DVS128_CONFIG_BIAS_DIFF], BIAS_LENGTH);
	integerToByteArray(271, state->biases[DVS128_CONFIG_BIAS_FOLL], BIAS_LENGTH);
	integerToByteArray(217, state->biases[DVS128_CONFIG_BIAS_PR], BIAS_LENGTH);

	// Send biases to device.
	dvs128SendBiases(state);

	return (true);
}

bool dvs128ConfigSet(caerDeviceHandle cdh, int8_t modAddr, uint8_t paramAddr, uint32_t param) {
	dvs128Handle handle = (dvs128Handle) cdh;
	dvs128State state = &handle->state;

	return (true);
}

bool dvs128ConfigGet(caerDeviceHandle cdh, int8_t modAddr, uint8_t paramAddr, uint32_t *param) {
	dvs128Handle handle = (dvs128Handle) cdh;
	dvs128State state = &handle->state;

	return (true);
}

bool dvs128DataStart(caerDeviceHandle cdh, void (*dataNotifyIncrease)(void *ptr), void (*dataNotifyDecrease)(void *ptr),
	void *dataNotifyUserPtr) {
	dvs128Handle handle = (dvs128Handle) cdh;
	dvs128State state = &handle->state;

	// Store new data available/not available anymore call-backs.
	state->dataNotifyIncrease = dataNotifyIncrease;
	state->dataNotifyDecrease = dataNotifyDecrease;
	state->dataNotifyUserPtr = dataNotifyUserPtr;

	// Initialize RingBuffer.
	state->dataExchangeBuffer = ringBufferInit(atomic_load(&state->dataExchangeBufferSize));
	if (state->dataExchangeBuffer == NULL) {
		caerLog(LOG_CRITICAL, handle->info.deviceString, "Failed to initialize data exchange buffer.");
		return (false);
	}

	// Allocate packets.
	state->currentPacketContainer = caerEventPacketContainerAllocate(EVENT_TYPES);
	if (state->currentPacketContainer == NULL) {
		freeAllDataMemory(state);

		caerLog(LOG_CRITICAL, handle->info.deviceString, "Failed to allocate event packet container.");
		return (false);
	}

	state->currentPolarityPacket = caerPolarityEventPacketAllocate(atomic_load(&state->maxPolarityPacketSize),
		(int16_t) handle->info.deviceID, 0);
	if (state->currentPolarityPacket == NULL) {
		freeAllDataMemory(state);

		caerLog(LOG_CRITICAL, handle->info.deviceString, "Failed to allocate polarity event packet.");
		return (false);
	}

	state->currentSpecialPacket = caerSpecialEventPacketAllocate(atomic_load(&state->maxSpecialPacketSize),
		(int16_t) handle->info.deviceID, 0);
	if (state->currentSpecialPacket == NULL) {
		freeAllDataMemory(state);

		caerLog(LOG_CRITICAL, handle->info.deviceString, "Failed to allocate special event packet.");
		return (false);
	}

	// Start data acquisition thread.
	if ((errno = pthread_create(&state->dataAcquisitionThread, NULL, &dvs128DataAcquisitionThread, handle)) != 0) {
		freeAllDataMemory(state);

		caerLog(LOG_CRITICAL, handle->info.deviceString, "Failed to start data acquisition thread. Error: %d.", errno);
		return (false);
	}

	return (true);
}

bool dvs128DataStop(caerDeviceHandle cdh) {
	dvs128Handle handle = (dvs128Handle) cdh;
	dvs128State state = &handle->state;

	// Wait for data acquisition thread to terminate...
	if ((errno = pthread_join(state->dataAcquisitionThread, NULL)) != 0) {
		// This should never happen!
		caerLog(LOG_CRITICAL, handle->info.deviceString, "Failed to join data acquisition thread. Error: %d.", errno);
		return (false);
	}

	// Empty ringbuffer.
	caerEventPacketContainer container;
	while ((container = ringBufferGet(state->dataExchangeBuffer)) != NULL) {
		// Notify data-not-available call-back.
		state->dataNotifyDecrease(state->dataNotifyUserPtr);

		// Free container, which will free its subordinate packets too.
		caerEventPacketContainerFree(container);
	}

	// Free current, uncommitted packets and ringbuffer.
	freeAllDataMemory(state);

	// Reset packet positions.
	state->currentPolarityPacketPosition = 0;
	state->currentSpecialPacketPosition = 0;

	return (true);
}

// Remember to properly free the returned memory after usage!
caerEventPacketContainer dvs128DataGet(caerDeviceHandle cdh) {
	dvs128Handle handle = (dvs128Handle) cdh;
	dvs128State state = &handle->state;
	caerEventPacketContainer container = NULL;

	retry: container = ringBufferGet(state->dataExchangeBuffer);

	if (container != NULL) {
		// Found an event container, return it and signal this piece of data
		// is no longer available for later acquisition.
		state->dataNotifyDecrease(state->dataNotifyUserPtr);

		return (container);
	}

	// Didn't find any event container, either report this or retry, depending
	// on blocking setting.
	if (atomic_load(&state->dataExchangeBlocking)) {
		goto retry;
	}

	// Nothing.
	return (NULL);
}

static libusb_device_handle *dvs128DeviceOpen(libusb_context *devContext, uint8_t busNumber, uint8_t devAddress) {
	libusb_device_handle *devHandle = NULL;
	libusb_device **devicesList;

	ssize_t result = libusb_get_device_list(devContext, &devicesList);

	if (result >= 0) {
		// Cycle thorough all discovered devices and find a match.
		for (size_t i = 0; i < (size_t) result; i++) {
			struct libusb_device_descriptor devDesc;

			if (libusb_get_device_descriptor(devicesList[i], &devDesc) != LIBUSB_SUCCESS) {
				continue;
			}

			// Check if this is the device we want (VID/PID).
			if (devDesc.idVendor == DEVICE_VID && devDesc.idProduct == DEVICE_PID
				&& (uint8_t) ((devDesc.bcdDevice & 0xFF00) >> 8) == DEVICE_DID_TYPE) {
				// If a USB port restriction is given, honor it.
				if (busNumber > 0 && libusb_get_bus_number(devicesList[i]) != busNumber) {
					continue;
				}

				if (devAddress > 0 && libusb_get_device_address(devicesList[i]) != devAddress) {
					continue;
				}

				if (libusb_open(devicesList[i], &devHandle) != LIBUSB_SUCCESS) {
					devHandle = NULL;

					continue;
				}

				// Check that the active configuration is set to number 1. If not, do so.
				int activeConfiguration;
				if (libusb_get_configuration(devHandle, &activeConfiguration) != LIBUSB_SUCCESS) {
					libusb_close(devHandle);
					devHandle = NULL;

					continue;
				}

				if (activeConfiguration != 1) {
					if (libusb_set_configuration(devHandle, 1) != LIBUSB_SUCCESS) {
						libusb_close(devHandle);
						devHandle = NULL;

						continue;
					}
				}

				// Claim interface 0 (default).
				if (libusb_claim_interface(devHandle, 0) != LIBUSB_SUCCESS) {
					libusb_close(devHandle);
					devHandle = NULL;

					continue;
				}

				// Found and configured it!
				break;
			}
		}

		libusb_free_device_list(devicesList, true);
	}

	return (devHandle);
}

static void dvs128DeviceClose(libusb_device_handle *devHandle) {
	// Release interface 0 (default).
	libusb_release_interface(devHandle, 0);

	libusb_close(devHandle);
}

static void dvs128AllocateTransfers(dvs128Handle handle, uint32_t bufferNum, uint32_t bufferSize) {
	dvs128State state = &handle->state;

	// Set number of transfers and allocate memory for the main transfer array.
	state->dataTransfers = calloc(bufferNum, sizeof(struct libusb_transfer *));
	if (state->dataTransfers == NULL) {
		caerLog(LOG_CRITICAL, handle->info.deviceString,
			"Failed to allocate memory for %" PRIu32 " libusb transfers. Error: %d.", bufferNum, errno);
		return;
	}
	state->dataTransfersLength = bufferNum;

	// Allocate transfers and set them up.
	for (size_t i = 0; i < bufferNum; i++) {
		state->dataTransfers[i] = libusb_alloc_transfer(0);
		if (state->dataTransfers[i] == NULL) {
			caerLog(LOG_CRITICAL, handle->info.deviceString,
				"Unable to allocate further libusb transfers (%zu of %" PRIu32 ").", i, bufferNum);
			continue;
		}

		// Create data buffer.
		state->dataTransfers[i]->length = (int) bufferSize;
		state->dataTransfers[i]->buffer = malloc(bufferSize);
		if (state->dataTransfers[i]->buffer == NULL) {
			caerLog(LOG_CRITICAL, handle->info.deviceString,
				"Unable to allocate buffer for libusb transfer %zu. Error: %d.", i, errno);

			libusb_free_transfer(state->dataTransfers[i]);
			state->dataTransfers[i] = NULL;

			continue;
		}

		// Initialize Transfer.
		state->dataTransfers[i]->dev_handle = state->deviceHandle;
		state->dataTransfers[i]->endpoint = DATA_ENDPOINT;
		state->dataTransfers[i]->type = LIBUSB_TRANSFER_TYPE_BULK;
		state->dataTransfers[i]->callback = &dvs128LibUsbCallback;
		state->dataTransfers[i]->user_data = handle;
		state->dataTransfers[i]->timeout = 0;
		state->dataTransfers[i]->flags = LIBUSB_TRANSFER_FREE_BUFFER;

		if ((errno = libusb_submit_transfer(state->dataTransfers[i])) == LIBUSB_SUCCESS) {
			state->activeDataTransfers++;
		}
		else {
			caerLog(LOG_CRITICAL, handle->info.deviceString, "Unable to submit libusb transfer %zu. Error: %s (%d).", i,
				libusb_strerror(errno), errno);

			// The transfer buffer is freed automatically here thanks to
			// the LIBUSB_TRANSFER_FREE_BUFFER flag set above.
			libusb_free_transfer(state->dataTransfers[i]);
			state->dataTransfers[i] = NULL;

			continue;
		}
	}

	if (state->activeDataTransfers == 0) {
		// Didn't manage to allocate any USB transfers, free array memory and log failure.
		free(state->dataTransfers);
		state->dataTransfers = NULL;
		state->dataTransfersLength = 0;

		caerLog(LOG_CRITICAL, handle->info.deviceString, "Unable to allocate any libusb transfers.");
	}
}

static void dvs128DeallocateTransfers(dvs128Handle handle) {
	dvs128State state = &handle->state;

	// Cancel all current transfers first.
	for (size_t i = 0; i < state->dataTransfersLength; i++) {
		if (state->dataTransfers[i] != NULL) {
			errno = libusb_cancel_transfer(state->dataTransfers[i]);
			if (errno != LIBUSB_SUCCESS && errno != LIBUSB_ERROR_NOT_FOUND) {
				caerLog(LOG_CRITICAL, handle->info.deviceString,
					"Unable to cancel libusb transfer %zu. Error: %s (%d).", i, libusb_strerror(errno), errno);
				// Proceed with trying to cancel all transfers regardless of errors.
			}
		}
	}

	// Wait for all transfers to go away (0.1 seconds timeout).
	struct timeval te = { .tv_sec = 0, .tv_usec = 100000 };

	while (state->activeDataTransfers > 0) {
		libusb_handle_events_timeout(state->deviceContext, &te);
	}

	// The buffers and transfers have been deallocated in the callback.
	// Only the transfers array remains, which we free here.
	free(state->dataTransfers);
	state->dataTransfers = NULL;
	state->dataTransfersLength = 0;
}

static void LIBUSB_CALL dvs128LibUsbCallback(struct libusb_transfer *transfer) {
	dvs128Handle handle = transfer->user_data;
	dvs128State state = &handle->state;

	if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
		// Handle data.
		dvs128EventTranslator(handle, transfer->buffer, (size_t) transfer->actual_length);
	}

	if (transfer->status != LIBUSB_TRANSFER_CANCELLED && transfer->status != LIBUSB_TRANSFER_NO_DEVICE) {
		// Submit transfer again.
		if (libusb_submit_transfer(transfer) == LIBUSB_SUCCESS) {
			return;
		}
	}

	// Cannot recover (cancelled, no device, or other critical error).
	// Signal this by adjusting the counter, free and exit.
	state->activeDataTransfers--;
	for (size_t i = 0; i < state->dataTransfersLength; i++) {
		// Remove from list, so we don't try to cancel it later on.
		if (state->dataTransfers[i] == transfer) {
			state->dataTransfers[i] = NULL;
		}
	}
	libusb_free_transfer(transfer);
}

#define DVS128_TIMESTAMP_WRAP_MASK 0x80
#define DVS128_TIMESTAMP_RESET_MASK 0x40
#define DVS128_POLARITY_SHIFT 0
#define DVS128_POLARITY_MASK 0x0001
#define DVS128_Y_ADDR_SHIFT 8
#define DVS128_Y_ADDR_MASK 0x007F
#define DVS128_X_ADDR_SHIFT 1
#define DVS128_X_ADDR_MASK 0x007F
#define DVS128_SYNC_EVENT_MASK 0x8000
#define TS_WRAP_ADD 0x4000

static void dvs128EventTranslator(dvs128Handle handle, uint8_t *buffer, size_t bytesSent) {
	dvs128State state = &handle->state;

	// Truncate off any extra partial event.
	if ((bytesSent & 0x03) != 0) {
		caerLog(LOG_ALERT, handle->info.deviceString, "%zu bytes sent via USB, which is not a multiple of four.",
			bytesSent);
		bytesSent &= (size_t) ~0x03;
	}

	for (size_t i = 0; i < bytesSent; i += 4) {
		// Allocate new packets for next iteration as needed.
		if (state->currentPacketContainer == NULL) {
			state->currentPacketContainer = caerEventPacketContainerAllocate(EVENT_TYPES);
			if (state->currentPacketContainer == NULL) {
				caerLog(LOG_CRITICAL, handle->info.deviceString, "Failed to allocate event packet container.");
				return;
			}
		}

		if (state->currentPolarityPacket == NULL) {
			state->currentPolarityPacket = caerPolarityEventPacketAllocate(atomic_load(&state->maxPolarityPacketSize),
				(int16_t) handle->info.deviceID, state->wrapOverflow);
			if (state->currentPolarityPacket == NULL) {
				caerLog(LOG_CRITICAL, handle->info.deviceString, "Failed to allocate polarity event packet.");
				return;
			}
		}

		if (state->currentSpecialPacket == NULL) {
			state->currentSpecialPacket = caerSpecialEventPacketAllocate(atomic_load(&state->maxSpecialPacketSize),
				(int16_t) handle->info.deviceID, state->wrapOverflow);
			if (state->currentSpecialPacket == NULL) {
				caerLog(LOG_CRITICAL, handle->info.deviceString, "Failed to allocate special event packet.");
				return;
			}
		}

		bool forceCommit = false;

		if ((buffer[i + 3] & DVS128_TIMESTAMP_WRAP_MASK) == DVS128_TIMESTAMP_WRAP_MASK) {
			// Detect big timestamp wrap-around.
			if (state->wrapAdd == (INT32_MAX - (TS_WRAP_ADD - 1))) {
				// Reset wrapAdd to zero at this point, so we can again
				// start detecting overruns of the 32bit value.
				state->wrapAdd = 0;

				state->lastTimestamp = 0;
				state->currentTimestamp = 0;

				// Increment TSOverflow counter.
				state->wrapOverflow++;

				caerSpecialEvent currentEvent = caerSpecialEventPacketGetEvent(state->currentSpecialPacket,
					state->currentSpecialPacketPosition++);
				caerSpecialEventSetTimestamp(currentEvent, INT32_MAX);
				caerSpecialEventSetType(currentEvent, TIMESTAMP_WRAP);
				caerSpecialEventValidate(currentEvent, state->currentSpecialPacket);

				// Commit packets to separate before wrap from after cleanly.
				forceCommit = true;
			}
			else {
				// timestamp bit 15 is one -> wrap: now we need to increment
				// the wrapAdd, uses only 14 bit timestamps. Each wrap is 2^14 µs (~16ms).
				state->wrapAdd += TS_WRAP_ADD;

				state->lastTimestamp = state->currentTimestamp;
				state->currentTimestamp = state->wrapAdd;

				// Check monotonicity of timestamps.
				checkMonotonicTimestamp(handle);
			}
		}
		else if ((buffer[i + 3] & DVS128_TIMESTAMP_RESET_MASK) == DVS128_TIMESTAMP_RESET_MASK) {
			// timestamp bit 14 is one -> wrapAdd reset: this firmware
			// version uses reset events to reset timestamps
			state->wrapOverflow = 0;
			state->wrapAdd = 0;
			state->lastTimestamp = 0;
			state->currentTimestamp = 0;

			// Create timestamp reset event.
			caerSpecialEvent currentEvent = caerSpecialEventPacketGetEvent(state->currentSpecialPacket,
				state->currentSpecialPacketPosition++);
			caerSpecialEventSetTimestamp(currentEvent, INT32_MAX);
			caerSpecialEventSetType(currentEvent, TIMESTAMP_RESET);
			caerSpecialEventValidate(currentEvent, state->currentSpecialPacket);

			// Commit packets when doing a reset to clearly separate them.
			forceCommit = true;
		}
		else {
			// address is LSB MSB (USB is LE)
			uint16_t addressUSB = le16toh(*((uint16_t *) (&buffer[i])));

			// same for timestamp, LSB MSB (USB is LE)
			// 15 bit value of timestamp in 1 us tick
			uint16_t timestampUSB = le16toh(*((uint16_t *) (&buffer[i + 2])));

			// Expand to 32 bits. (Tick is 1µs already.)
			state->lastTimestamp = state->currentTimestamp;
			state->currentTimestamp = state->wrapAdd + timestampUSB;

			// Check monotonicity of timestamps.
			checkMonotonicTimestamp(handle);

			if ((addressUSB & DVS128_SYNC_EVENT_MASK) != 0) {
				// Special Trigger Event (MSB is set)
				caerSpecialEvent currentEvent = caerSpecialEventPacketGetEvent(state->currentSpecialPacket,
					state->currentSpecialPacketPosition++);
				caerSpecialEventSetTimestamp(currentEvent, state->currentTimestamp);
				caerSpecialEventSetType(currentEvent, EXTERNAL_INPUT_RISING_EDGE);
				caerSpecialEventValidate(currentEvent, state->currentSpecialPacket);
			}
			else {
				// Invert x values (flip along the x axis).
				uint16_t x = (uint16_t) ((DVS_ARRAY_SIZE_X - 1)
					- ((uint16_t) ((addressUSB >> DVS128_X_ADDR_SHIFT) & DVS128_X_ADDR_MASK)));
				uint16_t y = (uint16_t) ((addressUSB >> DVS128_Y_ADDR_SHIFT) & DVS128_Y_ADDR_MASK);
				bool polarity = (((addressUSB >> DVS128_POLARITY_SHIFT) & DVS128_POLARITY_MASK) == 0) ? (1) : (0);

				// Check range conformity.
				if (x >= DVS_ARRAY_SIZE_X) {
					caerLog(LOG_ALERT, handle->info.deviceString, "X address out of range (0-%d): %" PRIu16 ".",
					DVS_ARRAY_SIZE_X - 1, x);
					continue; // Skip invalid event.
				}
				if (y >= DVS_ARRAY_SIZE_Y) {
					caerLog(LOG_ALERT, handle->info.deviceString, "Y address out of range (0-%d): %" PRIu16 ".",
					DVS_ARRAY_SIZE_Y - 1, y);
					continue; // Skip invalid event.
				}

				caerPolarityEvent currentEvent = caerPolarityEventPacketGetEvent(state->currentPolarityPacket,
					state->currentPolarityPacketPosition++);
				caerPolarityEventSetTimestamp(currentEvent, state->currentTimestamp);
				caerPolarityEventSetPolarity(currentEvent, polarity);
				caerPolarityEventSetY(currentEvent, y);
				caerPolarityEventSetX(currentEvent, x);
				caerPolarityEventValidate(currentEvent, state->currentPolarityPacket);
			}
		}

		// Thresholds on which to trigger packet container commit.
		// forceCommit is already defined above.
		int32_t polaritySize = state->currentPolarityPacketPosition;
		int32_t polarityInterval =
			(polaritySize > 1) ?
				(caerPolarityEventGetTimestamp(
					caerPolarityEventPacketGetEvent(state->currentPolarityPacket, polaritySize - 1))
					- caerPolarityEventGetTimestamp(caerPolarityEventPacketGetEvent(state->currentPolarityPacket, 0))) :
				(0);

		int32_t specialSize = state->currentSpecialPacketPosition;
		int32_t specialInterval =
			(specialSize > 1) ?
				(caerSpecialEventGetTimestamp(
					caerSpecialEventPacketGetEvent(state->currentSpecialPacket, specialSize - 1))
					- caerSpecialEventGetTimestamp(caerSpecialEventPacketGetEvent(state->currentSpecialPacket, 0))) :
				(0);

		// Trigger if any of the global container-wide thresholds are met.
		bool containerCommit = (((polaritySize + specialSize) >= atomic_load(&state->maxPacketContainerSize))
			|| ((polarityInterval + specialInterval) >= atomic_load(&state->maxPacketContainerInterval)));

		// Trigger if any of the packet-specific thresholds are met.
		bool polarityPacketCommit = ((polaritySize
			>= caerEventPacketHeaderGetEventCapacity(&state->currentPolarityPacket->packetHeader))
			|| (polarityInterval >= atomic_load(&state->maxPolarityPacketInterval)));

		// Trigger if any of the packet-specific thresholds are met.
		bool specialPacketCommit = ((specialSize
			>= caerEventPacketHeaderGetEventCapacity(&state->currentSpecialPacket->packetHeader))
			|| (specialInterval >= atomic_load(&state->maxSpecialPacketInterval)));

		// Commit packet containers to the ring-buffer, so they can be processed by the
		// main-loop, when any of the required conditions are met.
		if (forceCommit || containerCommit || polarityPacketCommit || specialPacketCommit) {
			// One or more of the commit triggers are hit. Set the packet container up to contain
			// any non-empty packets. Empty packets are not forwarded to save memory.
			if (polaritySize > 0) {
				caerEventPacketContainerSetEventPacket(state->currentPacketContainer, POLARITY_EVENT,
					(caerEventPacketHeader) state->currentPolarityPacket);

				state->currentPolarityPacket = NULL;
				state->currentPolarityPacketPosition = 0;
			}

			if (specialSize > 0) {
				caerEventPacketContainerSetEventPacket(state->currentPacketContainer, SPECIAL_EVENT,
					(caerEventPacketHeader) state->currentSpecialPacket);

				state->currentSpecialPacket = NULL;
				state->currentSpecialPacketPosition = 0;
			}

			retry_important: if (!ringBufferPut(state->dataExchangeBuffer, state->currentPacketContainer)) {
				// Failed to forward packet container, drop it, unless it contains a timestamp
				// related change, those are critical, so we just spin until we can
				// deliver that one. (Easily detected by forceCommit!)
				if (forceCommit) {
					goto retry_important;
				}
				else {
					// Failed to forward packet container, just drop it, it doesn't contain
					// any critical information anyway.
					caerLog(LOG_INFO, handle->info.deviceString,
						"Dropped EventPacket Container because ring-buffer full!");

					// Re-use the event-packet container to avoid having to reallocate it.
					// The contained event packets do have to be dropped first!
					caerEventPacketFree(
						caerEventPacketContainerGetEventPacket(state->currentPacketContainer, POLARITY_EVENT));
					caerEventPacketFree(
						caerEventPacketContainerGetEventPacket(state->currentPacketContainer, SPECIAL_EVENT));

					caerEventPacketContainerSetEventPacket(state->currentPacketContainer, POLARITY_EVENT, NULL);
					caerEventPacketContainerSetEventPacket(state->currentPacketContainer, SPECIAL_EVENT, NULL);
				}
			}
			else {
				state->dataNotifyIncrease(state->dataNotifyUserPtr);

				state->currentPacketContainer = NULL;
			}
		}
	}
}

static void dvs128SendBiases(dvs128State state) {
	// Biases are already stored in an array with the same format as expected by
	// the device, we can thus send it directly.

	libusb_control_transfer(state->deviceHandle,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
		VENDOR_REQUEST_SEND_BIASES, 0, 0, (uint8_t *) state->biases, BIAS_NUMBER * BIAS_LENGTH, 0);
}

static void *dvs128DataAcquisitionThread(void *inPtr) {
	// inPtr is a pointer to device handle.
	dvs128Handle handle = inPtr;
	dvs128State state = &handle->state;

	caerLog(LOG_DEBUG, handle->info.deviceString, "Initializing data acquisition thread ...");

	// Create buffers as specified in config file.
	dvs128AllocateTransfers(handle, atomic_load(&state->usbBufferNumber), atomic_load(&state->usbBufferSize));

	// Enable AER data transfer on USB end-point 6.
	libusb_control_transfer(state->deviceHandle,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
		VENDOR_REQUEST_START_TRANSFER, 0, 0, NULL, 0, 0);
	state->dvsRunning = true;

	// Handle USB events (1 second timeout).
	struct timeval te = { .tv_sec = 0, .tv_usec = 1000000 };

	caerLog(LOG_DEBUG, handle->info.deviceString, "data acquisition thread ready to process events.");

	while (atomic_load(&state->dataAcquisitionThreadRun) != 0 && state->activeDataTransfers > 0) {
		// Check config refresh, in this case to adjust buffer sizes.
		if (atomic_load(&state->dataAcquisitionThreadConfigUpdate) != 0) {
			dvs128DataAcquisitionThreadConfig(handle);
		}

		libusb_handle_events_timeout(state->deviceContext, &te);
	}

	caerLog(LOG_DEBUG, handle->info.deviceString, "shutting down data acquisition thread ...");

	// Disable AER data transfer on USB end-point 6.
	libusb_control_transfer(state->deviceHandle,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
		VENDOR_REQUEST_STOP_TRANSFER, 0, 0, NULL, 0, 0);
	state->dvsRunning = false;

	// Cancel all transfers and handle them.
	dvs128DeallocateTransfers(handle);

	caerLog(LOG_DEBUG, handle->info.deviceString, "data acquisition thread shut down.");

	return (NULL);
}

static void dvs128DataAcquisitionThreadConfig(dvs128Handle handle) {
	dvs128State state = &handle->state;

	// Get the current value to examine by atomic exchange, since we don't
	// want there to be any possible store between a load/store pair.
	uint32_t configUpdate = atomic_exchange(&state->dataAcquisitionThreadConfigUpdate, 0);

	if (configUpdate & (0x01 << 0)) {
		// Bias update required.
		dvs128SendBiases(state);
	}

	if (configUpdate & (0x01 << 1)) {
		// Do buffer size change: cancel all and recreate them.
		dvs128DeallocateTransfers(handle);
		dvs128AllocateTransfers(handle, atomic_load(&state->usbBufferNumber), atomic_load(&state->usbBufferSize));
	}
}
