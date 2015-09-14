#include "dvs128.h"

static libusb_device_handle *dvs128DeviceOpen(libusb_context *devContext, uint8_t busNumber, uint8_t devAddress);
static void dvs128DeviceClose(libusb_device_handle *devHandle);
static void dvs128AllocateTransfers(dvs128State state, uint32_t bufferNum, uint32_t bufferSize);
static void dvs128DeallocateTransfers(dvs128State state);
static void LIBUSB_CALL dvs128LibUsbCallback(struct libusb_transfer *transfer);
static void dvs128EventTranslator(dvs128State state, uint8_t *buffer, size_t bytesSent);
static void dvs128SendBiases(dvs128State state);

caerDeviceHandle dvs128Open(uint16_t deviceID, uint8_t busNumberRestrict, uint8_t devAddressRestrict,
	const char *serialNumberRestrict) {
	dvs128Handle handle = calloc(1, sizeof(*handle));
	if (handle == NULL) {
		// Failed to allocate memory for device handle!
		caerLog(LOG_CRITICAL, __func__, "Failed to allocate memory for device handle.");

		return (NULL);
	}

	dvs128State state = &handle->state;

	// Initialize state variables to default values (if not zero, taken care of by calloc above).
	state->deviceID = deviceID;
	atomic_store(&state->dataExchangeBufferSize, 64);
	atomic_store(&state->dataExchangeBlocking, false);
	atomic_store(&state->usbBufferNumber, 8);
	atomic_store(&state->usbBufferSize, 4096);
	atomic_store(&state->maxPacketContainerSize, 4096 + 128);
	atomic_store(&state->maxPacketContainerInterval, 5000);
	atomic_store(&state->maxPolarityPacketSize, 4096);
	atomic_store(&state->maxPolarityPacketInterval, 5000);
	atomic_store(&state->maxSpecialPacketSize, 128);
	atomic_store(&state->maxSpecialPacketInterval, 1000);

	// Search for device and open it.

	// Populate info variables based on data from device.

	return ((caerDeviceHandle) handle);
}

bool dvs128Close(caerDeviceHandle cdh) {
	dvs128Handle handle = (dvs128Handle) cdh;
	dvs128State state = &handle->state;

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
}

bool dvs128ConfigSet(caerDeviceHandle cdh, int8_t modAddr, uint8_t paramAddr, uint32_t param) {
	dvs128Handle handle = (dvs128Handle) cdh;
	dvs128State state = &handle->state;
}

bool dvs128ConfigGet(caerDeviceHandle cdh, int8_t modAddr, uint8_t paramAddr, uint32_t *param) {
	dvs128Handle handle = (dvs128Handle) cdh;
	dvs128State state = &handle->state;
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

	}
}

bool dvs128DataStop(caerDeviceHandle cdh) {
	dvs128Handle handle = (dvs128Handle) cdh;
	dvs128State state = &handle->state;

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
	if (state->dataExchangeBlocking) {
		// TODO: think about thread-safety.
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
			if (devDesc.idVendor == DVS128_VID && devDesc.idProduct == DVS128_PID
				&& (uint8_t) ((devDesc.bcdDevice & 0xFF00) >> 8) == DVS128_DID_TYPE) {
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

static void dvs128AllocateTransfers(dvs128State state, uint32_t bufferNum, uint32_t bufferSize) {
	// Set number of transfers and allocate memory for the main transfer array.
	state->dataTransfers = calloc(bufferNum, sizeof(struct libusb_transfer *));
	if (state->dataTransfers == NULL) {
		caerLog(LOG_CRITICAL, __func__, "Failed to allocate memory for %" PRIu32 " libusb transfers. Error: %d.",
			bufferNum, errno);
		return;
	}
	state->dataTransfersLength = bufferNum;

	// Allocate transfers and set them up.
	for (size_t i = 0; i < bufferNum; i++) {
		state->dataTransfers[i] = libusb_alloc_transfer(0);
		if (state->dataTransfers[i] == NULL) {
			caerLog(LOG_CRITICAL, __func__, "Unable to allocate further libusb transfers (%zu of %" PRIu32 ").", i,
				bufferNum);
			continue;
		}

		// Create data buffer.
		state->dataTransfers[i]->length = (int) bufferSize;
		state->dataTransfers[i]->buffer = malloc(bufferSize);
		if (state->dataTransfers[i]->buffer == NULL) {
			caerLog(LOG_CRITICAL, __func__, "Unable to allocate buffer for libusb transfer %zu. Error: %d.", i, errno);

			libusb_free_transfer(state->dataTransfers[i]);
			state->dataTransfers[i] = NULL;

			continue;
		}

		// Initialize Transfer.
		state->dataTransfers[i]->dev_handle = state->deviceHandle;
		state->dataTransfers[i]->endpoint = DATA_ENDPOINT;
		state->dataTransfers[i]->type = LIBUSB_TRANSFER_TYPE_BULK;
		state->dataTransfers[i]->callback = &dvs128LibUsbCallback;
		state->dataTransfers[i]->user_data = state;
		state->dataTransfers[i]->timeout = 0;
		state->dataTransfers[i]->flags = LIBUSB_TRANSFER_FREE_BUFFER;

		if ((errno = libusb_submit_transfer(state->dataTransfers[i])) == LIBUSB_SUCCESS) {
			state->activeDataTransfers++;
		}
		else {
			caerLog(LOG_CRITICAL, __func__, "Unable to submit libusb transfer %zu. Error: %s (%d).", i,
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

		caerLog(LOG_CRITICAL, __func__, "Unable to allocate any libusb transfers.");
	}
}

static void dvs128DeallocateTransfers(dvs128State state) {
	// Cancel all current transfers first.
	for (size_t i = 0; i < state->dataTransfersLength; i++) {
		if (state->dataTransfers[i] != NULL) {
			errno = libusb_cancel_transfer(state->dataTransfers[i]);
			if (errno != LIBUSB_SUCCESS && errno != LIBUSB_ERROR_NOT_FOUND) {
				caerLog(LOG_CRITICAL, __func__, "Unable to cancel libusb transfer %zu. Error: %s (%d).", i,
					libusb_strerror(errno), errno);
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
	dvs128State state = transfer->user_data;

	if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
		// Handle data.
		dvs128EventTranslator(state, transfer->buffer, (size_t) transfer->actual_length);
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

#define DVS128_POLARITY_SHIFT 0
#define DVS128_POLARITY_MASK 0x0001
#define DVS128_Y_ADDR_SHIFT 8
#define DVS128_Y_ADDR_MASK 0x007F
#define DVS128_X_ADDR_SHIFT 1
#define DVS128_X_ADDR_MASK 0x007F
#define DVS128_SYNC_EVENT_MASK 0x8000

static void dvs128EventTranslator(dvs128State state, uint8_t *buffer, size_t bytesSent) {
	// Truncate off any extra partial event.
	if ((bytesSent & 0x03) != 0) {
		caerLog(LOG_ALERT, __func__, "%zu bytes sent via USB, which is not a multiple of four.", bytesSent);
		bytesSent &= (size_t) ~0x03;
	}

	for (size_t i = 0; i < bytesSent; i += 4) {
		bool forcePacketCommit = false;

		if ((buffer[i + 3] & 0x80) == 0x80) {
			// timestamp bit 15 is one -> wrap: now we need to increment
			// the wrapAdd, uses only 14 bit timestamps
			state->wrapAdd += 0x4000;

			// Detect big timestamp wrap-around.
			if (state->wrapAdd == 0) {
				// Reset lastTimestamp to zero at this point, so we can again
				// start detecting overruns of the 32bit value.
				state->lastTimestamp = 0;

				caerSpecialEvent currentEvent = caerSpecialEventPacketGetEvent(state->currentSpecialPacket,
					state->currentSpecialPacketPosition++);
				caerSpecialEventSetTimestamp(currentEvent, UINT32_MAX);
				caerSpecialEventSetType(currentEvent, TIMESTAMP_WRAP);
				caerSpecialEventValidate(currentEvent, state->currentSpecialPacket);

				// Commit packets to separate before wrap from after cleanly.
				forcePacketCommit = true;
			}
		}
		else if ((buffer[i + 3] & 0x40) == 0x40) {
			// timestamp bit 14 is one -> wrapAdd reset: this firmware
			// version uses reset events to reset timestamps
			state->wrapAdd = 0;
			state->lastTimestamp = 0;

			// Create timestamp reset event.
			caerSpecialEvent currentEvent = caerSpecialEventPacketGetEvent(state->currentSpecialPacket,
				state->currentSpecialPacketPosition++);
			caerSpecialEventSetTimestamp(currentEvent, UINT32_MAX);
			caerSpecialEventSetType(currentEvent, TIMESTAMP_RESET);
			caerSpecialEventValidate(currentEvent, state->currentSpecialPacket);

			// Commit packets when doing a reset to clearly separate them.
			forcePacketCommit = true;
		}
		else {
			// address is LSB MSB (USB is LE)
			uint16_t addressUSB = le16toh(*((uint16_t *) (&buffer[i])));

			// same for timestamp, LSB MSB (USB is LE)
			// 15 bit value of timestamp in 1 us tick
			uint16_t timestampUSB = le16toh(*((uint16_t *) (&buffer[i + 2])));

			// Expand to 32 bits. (Tick is 1µs already.)
			int32_t timestamp = timestampUSB + state->wrapAdd;

			// Check monotonicity of timestamps.
			if (timestamp < state->lastTimestamp) {
				caerLog(LOG_ALERT, __func__,
					"non-monotonic time-stamp detected: lastTimestamp=%" PRIu32 ", timestamp=%" PRIu32 ".",
					state->lastTimestamp, timestamp);
			}

			state->lastTimestamp = timestamp;

			if ((addressUSB & DVS128_SYNC_EVENT_MASK) != 0) {
				// Special Trigger Event (MSB is set)
				caerSpecialEvent currentEvent = caerSpecialEventPacketGetEvent(state->currentSpecialPacket,
					state->currentSpecialPacketPosition++);
				caerSpecialEventSetTimestamp(currentEvent, timestamp);
				caerSpecialEventSetType(currentEvent, EXTERNAL_INPUT_RISING_EDGE);
				caerSpecialEventValidate(currentEvent, state->currentSpecialPacket);
			}
			else {
				// Invert x values (flip along the x axis).
				uint16_t x = (uint16_t) (127 - ((uint16_t) ((addressUSB >> DVS128_X_ADDR_SHIFT) & DVS128_X_ADDR_MASK)));
				uint16_t y = (uint16_t) ((addressUSB >> DVS128_Y_ADDR_SHIFT) & DVS128_Y_ADDR_MASK);
				bool polarity = (((addressUSB >> DVS128_POLARITY_SHIFT) & DVS128_POLARITY_MASK) == 0) ? (1) : (0);

				// Check range conformity.
				if (x >= DVS128_ARRAY_SIZE_X) {
					caerLog(LOG_ALERT, __func__, "X address out of range (0-%d): %" PRIu16 ".",
					DVS128_ARRAY_SIZE_X - 1, x);
					continue; // Skip invalid event.
				}
				if (y >= DVS128_ARRAY_SIZE_Y) {
					caerLog(LOG_ALERT, __func__, "Y address out of range (0-%d): %" PRIu16 ".",
					DVS128_ARRAY_SIZE_Y - 1, y);
					continue; // Skip invalid event.
				}

				caerPolarityEvent currentEvent = caerPolarityEventPacketGetEvent(state->currentPolarityPacket,
					state->currentPolarityPacketPosition++);
				caerPolarityEventSetTimestamp(currentEvent, timestamp);
				caerPolarityEventSetPolarity(currentEvent, polarity);
				caerPolarityEventSetY(currentEvent, y);
				caerPolarityEventSetX(currentEvent, x);
				caerPolarityEventValidate(currentEvent, state->currentPolarityPacket);
			}
		}

		// Commit packet to the ring-buffer, so they can be processed by the
		// main-loop, when their stated conditions are met.
		if (forcePacketCommit
			|| (state->currentPolarityPacketPosition
				>= caerEventPacketHeaderGetEventCapacity(&state->currentPolarityPacket->packetHeader))
			|| ((state->currentPolarityPacketPosition > 1)
				&& (caerPolarityEventGetTimestamp(
					caerPolarityEventPacketGetEvent(state->currentPolarityPacket,
						state->currentPolarityPacketPosition - 1))
					- caerPolarityEventGetTimestamp(caerPolarityEventPacketGetEvent(state->currentPolarityPacket, 0))
					>= state->maxPolarityPacketInterval))) {
			if (!ringBufferPut(state->dataExchangeBuffer, state->currentPolarityPacket)) {
				// Failed to forward packet, drop it.
				free(state->currentPolarityPacket);
				caerLog(LOG_INFO, __func__, "Dropped Polarity Event Packet because ring-buffer full!");
			}
			else {
				state->dataNotifyIncrease(state->dataNotifyUserPtr);
			}

			// Allocate new packet for next iteration.
			state->currentPolarityPacket = caerPolarityEventPacketAllocate(state->maxPolarityPacketSize,
				state->deviceID);
			state->currentPolarityPacketPosition = 0;
		}

		if (forcePacketCommit
			|| (state->currentSpecialPacketPosition
				>= caerEventPacketHeaderGetEventCapacity(&state->currentSpecialPacket->packetHeader))
			|| ((state->currentSpecialPacketPosition > 1)
				&& (caerSpecialEventGetTimestamp(
					caerSpecialEventPacketGetEvent(state->currentSpecialPacket,
						state->currentSpecialPacketPosition - 1))
					- caerSpecialEventGetTimestamp(caerSpecialEventPacketGetEvent(state->currentSpecialPacket, 0))
					>= state->maxSpecialPacketInterval))) {
			retry_special: if (!ringBufferPut(state->dataExchangeBuffer, state->currentSpecialPacket)) {
				// Failed to forward packet, drop it, unless it contains a timestamp
				// related change, those are critical, so we just spin until we can
				// deliver that one. (Easily detected by forcePacketCommit!)
				if (forcePacketCommit) {
					goto retry_special;
				}
				else {
					// Failed to forward packet, drop it.
					free(state->currentSpecialPacket);
					caerLog(LOG_INFO, __func__, "Dropped Special Event Packet because ring-buffer full!");
				}
			}
			else {
				state->dataNotifyIncrease(state->dataNotifyUserPtr);
			}

			// Allocate new packet for next iteration.
			state->currentSpecialPacket = caerSpecialEventPacketAllocate(state->maxSpecialPacketSize, state->deviceID);
			state->currentSpecialPacketPosition = 0;
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

static void *dvs128DataAcquisitionThread(void *inPtr);
static void dvs128DataAcquisitionThreadConfig(caerModuleData data);

static inline void freeAllMemory(dvs128State state) {
	if (state->currentPolarityPacket != NULL) {
		free(state->currentPolarityPacket);
		state->currentPolarityPacket = NULL;
	}

	if (state->currentSpecialPacket != NULL) {
		free(state->currentSpecialPacket);
		state->currentSpecialPacket = NULL;
	}

	if (state->dataExchangeBuffer != NULL) {
		ringBufferFree(state->dataExchangeBuffer);
		state->dataExchangeBuffer = NULL;
	}
}

static bool caerInputDVS128Init(caerModuleData moduleData) {
	caerLog(LOG_DEBUG, moduleData->moduleSubSystemString, "Initializing module ...");

	// Packet settings (size (in events) and time interval (in µs)).
	sshsNodePutIntIfAbsent(moduleData->moduleNode, "polarityPacketMaxSize", 4096);
	sshsNodePutIntIfAbsent(moduleData->moduleNode, "polarityPacketMaxInterval", 5000);
	sshsNodePutIntIfAbsent(moduleData->moduleNode, "specialPacketMaxSize", 128);
	sshsNodePutIntIfAbsent(moduleData->moduleNode, "specialPacketMaxInterval", 1000);

	// Install default listener to signal configuration updates asynchronously.
	sshsNodeAddAttrListener(biasNode, moduleData, &caerInputDVS128ConfigListener);
	sshsNodeAddAttrListener(moduleData->moduleNode, moduleData, &caerInputDVS128ConfigListener);

	dvs128State state = moduleData->moduleState;

	// Data source is the same as the module ID (but accessible in state-space).
	state->sourceID = moduleData->moduleID;
	state->sourceSubSystemString = moduleData->moduleSubSystemString;

	// Put global source information into SSHS.
	sshsNode sourceInfoNode = sshsGetRelativeNode(moduleData->moduleNode, "sourceInfo/");
	sshsNodePutShort(sourceInfoNode, "dvsSizeX", DVS128_ARRAY_SIZE_X);
	sshsNodePutShort(sourceInfoNode, "dvsSizeY", DVS128_ARRAY_SIZE_Y);

	// Store reference to parent mainloop, so that we can correctly notify
	// the availability or not of data to consume.
	state->mainloopNotify = caerMainloopGetReference();

	// Create data exchange buffers.
	state->dataExchangeBuffer = ringBufferInit(sshsNodeGetInt(moduleData->moduleNode, "dataExchangeBufferSize"));
	if (state->dataExchangeBuffer == NULL) {
		freeAllMemory(state);

		caerLog(LOG_CRITICAL, moduleData->moduleSubSystemString, "Failed to initialize data exchange buffer.");
		return (false);
	}

	// Initialize libusb using a separate context for each device.
	// This is to correctly support one thread per device.
	if ((errno = libusb_init(&state->deviceContext)) != LIBUSB_SUCCESS) {
		freeAllMemory(state);

		caerLog(LOG_CRITICAL, moduleData->moduleSubSystemString, "Failed to initialize libusb context. Error: %s (%d).",
			libusb_strerror(errno), errno);
		return (false);
	}

	// Try to open a DVS128 device on a specific USB port.
	state->deviceHandle = dvs128Open(state->deviceContext, sshsNodeGetByte(moduleData->moduleNode, "usbBusNumber"),
		sshsNodeGetByte(moduleData->moduleNode, "usbDevAddress"));
	if (state->deviceHandle == NULL) {
		freeAllMemory(state);
		libusb_exit(state->deviceContext);

		caerLog(LOG_CRITICAL, moduleData->moduleSubSystemString, "Failed to open DVS128 device.");
		return (false);
	}

	// Start data acquisition thread.
	if ((errno = pthread_create(&state->dataAcquisitionThread, NULL, &dvs128DataAcquisitionThread, moduleData)) != 0) {
		freeAllMemory(state);
		dvs128Close(state->deviceHandle);
		libusb_exit(state->deviceContext);

		caerLog(LOG_CRITICAL, moduleData->moduleSubSystemString,
			"Failed to start data acquisition thread. Error: %s (%d).", caerLogStrerror(errno),
			errno);
		return (false);
	}

	caerLog(LOG_DEBUG, moduleData->moduleSubSystemString,
		"Initialized module successfully with device Bus=%" PRIu8 ":Addr=%" PRIu8 ".",
		libusb_get_bus_number(libusb_get_device(state->deviceHandle)),
		libusb_get_device_address(libusb_get_device(state->deviceHandle)));
	return (true);
}

static void caerInputDVS128Exit(caerModuleData moduleData) {
	caerLog(LOG_DEBUG, moduleData->moduleSubSystemString, "Shutting down ...");

	dvs128State state = moduleData->moduleState;

	// Wait for data acquisition thread to terminate...
	if ((errno = pthread_join(state->dataAcquisitionThread, NULL)) != 0) {
		// This should never happen!
		caerLog(LOG_CRITICAL, moduleData->moduleSubSystemString,
			"Failed to join data acquisition thread. Error: %s (%d).", caerLogStrerror(errno), errno);
	}

	// Finally, close the device fully.
	dvs128Close(state->deviceHandle);

	// Destroy libusb context.
	libusb_exit(state->deviceContext);

	// Empty ringbuffer.
	void *packet;
	while ((packet = ringBufferGet(state->dataExchangeBuffer)) != NULL) {
		caerMainloopDataAvailableDecrease(state->mainloopNotify);
		free(packet);
	}

	// Free packets and ringbuffer.
	freeAllMemory(state);

	caerLog(LOG_DEBUG, moduleData->moduleSubSystemString, "Shutdown successful.");
}

static void *dvs128DataAcquisitionThread(void *inPtr) {
	// inPtr is a pointer to module data.
	caerModuleData data = inPtr;
	dvs128State state = data->moduleState;

	caerLog(LOG_DEBUG, data->moduleSubSystemString, "Initializing data acquisition thread ...");

	// Send default start-up biases to device before enabling it.
	dvs128SendBiases(sshsGetRelativeNode(data->moduleNode, "bias/"), state->deviceHandle);

	// Create buffers as specified in config file.
	dvs128AllocateTransfers(state, sshsNodeGetInt(data->moduleNode, "bufferNumber"),
		sshsNodeGetInt(data->moduleNode, "bufferSize"));

	// Enable AER data transfer on USB end-point 6.
	libusb_control_transfer(state->deviceHandle,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
		VENDOR_REQUEST_START_TRANSFER, 0, 0, NULL, 0, 0);

	// Handle USB events (1 second timeout).
	struct timeval te = { .tv_sec = 0, .tv_usec = 1000000 };

	caerLog(LOG_DEBUG, data->moduleSubSystemString, "data acquisition thread ready to process events.");

	while (atomic_ops_uint_load(&data->running, ATOMIC_OPS_FENCE_NONE) != 0 && state->activeTransfers > 0) {
		// Check config refresh, in this case to adjust buffer sizes.
		if (atomic_ops_uint_load(&data->configUpdate, ATOMIC_OPS_FENCE_NONE) != 0) {
			dvs128DataAcquisitionThreadConfig(data);
		}

		libusb_handle_events_timeout(state->deviceContext, &te);
	}

	caerLog(LOG_DEBUG, data->moduleSubSystemString, "shutting down data acquisition thread ...");

	// Disable AER data transfer on USB end-point 6.
	libusb_control_transfer(state->deviceHandle,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
		VENDOR_REQUEST_STOP_TRANSFER, 0, 0, NULL, 0, 0);

	// Cancel all transfers and handle them.
	dvs128DeallocateTransfers(state);

	// Ensure parent also shuts down (on disconnected device for example).
	sshsNodePutBool(data->moduleNode, "shutdown", true);

	caerLog(LOG_DEBUG, data->moduleSubSystemString, "data acquisition thread shut down.");

	return (NULL);
}

static void dvs128DataAcquisitionThreadConfig(caerModuleData moduleData) {
	dvs128State state = moduleData->moduleState;

	// Get the current value to examine by atomic exchange, since we don't
	// want there to be any possible store between a load/store pair.
	uintptr_t configUpdate = atomic_ops_uint_swap(&moduleData->configUpdate, 0, ATOMIC_OPS_FENCE_NONE);

	if (configUpdate & (0x01 << 0)) {
		// Bias update required.
		dvs128SendBiases(sshsGetRelativeNode(moduleData->moduleNode, "bias/"), state->deviceHandle);
	}

	if (configUpdate & (0x01 << 1)) {
		// Do buffer size change: cancel all and recreate them.
		dvs128DeallocateTransfers(state);
		dvs128AllocateTransfers(state, sshsNodeGetInt(moduleData->moduleNode, "bufferNumber"),
			sshsNodeGetInt(moduleData->moduleNode, "bufferSize"));
	}

	if (configUpdate & (0x01 << 2)) {
		// Update maximum size and interval settings for packets.
		state->maxPolarityPacketSize = sshsNodeGetInt(moduleData->moduleNode, "polarityPacketMaxSize");
		state->maxPolarityPacketInterval = sshsNodeGetInt(moduleData->moduleNode, "polarityPacketMaxInterval");

		state->maxSpecialPacketSize = sshsNodeGetInt(moduleData->moduleNode, "specialPacketMaxSize");
		state->maxSpecialPacketInterval = sshsNodeGetInt(moduleData->moduleNode, "specialPacketMaxInterval");
	}
}
