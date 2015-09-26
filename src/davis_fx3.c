#include "davis_fx3.h"

caerDeviceHandle davisFX3Open(uint16_t deviceID, uint8_t busNumberRestrict, uint8_t devAddressRestrict,
	const char *serialNumberRestrict) {
	caerLog(CAER_LOG_DEBUG, __func__, "Initializing " DEVICE_NAME ".");

	davisFX3Handle handle = calloc(1, sizeof(*handle));
	if (handle == NULL) {
		// Failed to allocate memory for device handle!
		caerLog(CAER_LOG_CRITICAL, __func__, "Failed to allocate memory for device handle.");
		return (NULL);
	}

	bool openRetVal = davisCommonOpen((davisHandle) handle, DEVICE_VID, DEVICE_PID, DEVICE_DID_TYPE, DEVICE_NAME,
		deviceID, busNumberRestrict, devAddressRestrict, serialNumberRestrict, REQUIRED_LOGIC_REVISION);
	if (!openRetVal) {
		// Failed to open device and grab basic information!
		caerLog(CAER_LOG_CRITICAL, __func__, "Failed to open device.");
		return (NULL);
	}

	// TODO: allocate/deallocate debug transfers.

	return ((caerDeviceHandle) handle);
}

bool davisFX3SendDefaultConfig(caerDeviceHandle cdh) {
	davisHandle handle = (davisHandle) cdh;

}

bool davisFX3ConfigSet(caerDeviceHandle cdh, int8_t modAddr, uint8_t paramAddr, uint32_t param) {
	davisHandle handle = (davisHandle) cdh;

}

bool davisFX3ConfigGet(caerDeviceHandle cdh, int8_t modAddr, uint8_t paramAddr, uint32_t *param) {
	davisHandle handle = (davisHandle) cdh;

}

static void allocateDebugTransfers(davisFX3Handle handle);
static void deallocateDebugTransfers(davisFX3Handle handle);
static void LIBUSB_CALL libUsbDebugCallback(struct libusb_transfer *transfer);
static void debugTranslator(davisFX3Handle handle, uint8_t *buffer, size_t bytesSent);

static void allocateDebugTransfers(davisFX3Handle handle) {
	// Set number of transfers and allocate memory for the main transfer array.

	// Allocate transfers and set them up.
	for (size_t i = 0; i < DEBUG_TRANSFER_NUM; i++) {
		handle->debugTransfers[i] = libusb_alloc_transfer(0);
		if (handle->debugTransfers[i] == NULL) {
			caerLog(CAER_LOG_CRITICAL, handle->h.info.deviceString,
				"Unable to allocate further libusb transfers (debug channel, %zu of %" PRIu32 ").", i,
				DEBUG_TRANSFER_NUM);
			continue;
		}

		// Create data buffer.
		handle->debugTransfers[i]->length = DEBUG_TRANSFER_SIZE;
		handle->debugTransfers[i]->buffer = malloc(DEBUG_TRANSFER_SIZE);
		if (handle->debugTransfers[i]->buffer == NULL) {
			caerLog(CAER_LOG_CRITICAL, handle->h.info.deviceString,
				"Unable to allocate buffer for libusb transfer %zu (debug channel). Error: %d.", i, errno);

			libusb_free_transfer(handle->debugTransfers[i]);
			handle->debugTransfers[i] = NULL;

			continue;
		}

		// Initialize Transfer.
		handle->debugTransfers[i]->dev_handle = handle->h.state.deviceHandle;
		handle->debugTransfers[i]->endpoint = DEBUG_ENDPOINT;
		handle->debugTransfers[i]->type = LIBUSB_TRANSFER_TYPE_INTERRUPT;
		handle->debugTransfers[i]->callback = &libUsbDebugCallback;
		handle->debugTransfers[i]->user_data = handle;
		handle->debugTransfers[i]->timeout = 0;
		handle->debugTransfers[i]->flags = LIBUSB_TRANSFER_FREE_BUFFER;

		if ((errno = libusb_submit_transfer(handle->debugTransfers[i])) == LIBUSB_SUCCESS) {
			handle->activeDebugTransfers++;
		}
		else {
			caerLog(CAER_LOG_CRITICAL, handle->h.info.deviceString,
				"Unable to submit libusb transfer %zu (debug channel). Error: %s (%d).", i, libusb_strerror(errno),
				errno);

			// The transfer buffer is freed automatically here thanks to
			// the LIBUSB_TRANSFER_FREE_BUFFER flag set above.
			libusb_free_transfer(handle->debugTransfers[i]);
			handle->debugTransfers[i] = NULL;

			continue;
		}
	}

	if (handle->activeDebugTransfers == 0) {
		// Didn't manage to allocate any USB transfers, log failure.
		caerLog(CAER_LOG_CRITICAL, handle->h.info.deviceString, "Unable to allocate any libusb transfers.");
	}
}

static void deallocateDebugTransfers(davisFX3Handle handle) {
	// Cancel all current transfers first.
	for (size_t i = 0; i < DEBUG_TRANSFER_NUM; i++) {
		if (handle->debugTransfers[i] != NULL) {
			errno = libusb_cancel_transfer(handle->debugTransfers[i]);
			if (errno != LIBUSB_SUCCESS && errno != LIBUSB_ERROR_NOT_FOUND) {
				caerLog(CAER_LOG_CRITICAL, handle->h.info.deviceString,
					"Unable to cancel libusb transfer %zu (debug channel). Error: %s (%d).", i, libusb_strerror(errno),
					errno);
				// Proceed with trying to cancel all transfers regardless of errors.
			}
		}
	}

	// Wait for all transfers to go away (0.1 seconds timeout).
	struct timeval te = { .tv_sec = 0, .tv_usec = 100000 };

	while (handle->activeDebugTransfers > 0) {
		libusb_handle_events_timeout(handle->h.state.deviceContext, &te);
	}
}

static void LIBUSB_CALL libUsbDebugCallback(struct libusb_transfer *transfer) {
	davisFX3Handle handle = transfer->user_data;

	if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
		// Handle debug data.
		debugTranslator(handle, transfer->buffer, (size_t) transfer->actual_length);
	}

	if (transfer->status != LIBUSB_TRANSFER_CANCELLED && transfer->status != LIBUSB_TRANSFER_NO_DEVICE) {
		// Submit transfer again.
		if (libusb_submit_transfer(transfer) == LIBUSB_SUCCESS) {
			return;
		}
	}

	// Cannot recover (cancelled, no device, or other critical error).
	// Signal this by adjusting the counter, free and exit.
	handle->activeDebugTransfers--;
	for (size_t i = 0; i < DEBUG_TRANSFER_NUM; i++) {
		// Remove from list, so we don't try to cancel it later on.
		if (handle->debugTransfers[i] == transfer) {
			handle->debugTransfers[i] = NULL;
		}
	}
	libusb_free_transfer(transfer);
}

static void debugTranslator(davisFX3Handle handle, uint8_t *buffer, size_t bytesSent) {
	// Check if this is a debug message (length 7-64 bytes).
	if (bytesSent >= 7 && buffer[0] == 0x00) {
		// Debug message, log this.
		caerLog(CAER_LOG_ERROR, handle->h.info.deviceString, "Error message: '%s' (code %u at time %u).", &buffer[6],
			buffer[1], *((uint32_t *) &buffer[2]));
	}
	else {
		// Unknown/invalid debug message, log this.
		caerLog(CAER_LOG_WARNING, handle->h.info.deviceString, "Unknown/invalid debug message.");
	}
}

static void sendBiases(sshsNode moduleNode, davisCommonState cstate);
static void sendChipSR(sshsNode moduleNode, davisCommonState cstate);
static void BiasesListener(sshsNode node, void *userData, enum sshs_node_attribute_events event, const char *changeKey,
	enum sshs_node_attr_value_type changeType, union sshs_node_attr_value changeValue);
static void ChipSRListener(sshsNode node, void *userData, enum sshs_node_attribute_events event, const char *changeKey,
	enum sshs_node_attr_value_type changeType, union sshs_node_attr_value changeValue);

static void BiasesListener(sshsNode node, void *userData, enum sshs_node_attribute_events event, const char *changeKey,
	enum sshs_node_attr_value_type changeType, union sshs_node_attr_value changeValue) {
	UNUSED_ARGUMENT(changeKey);
	UNUSED_ARGUMENT(changeType);
	UNUSED_ARGUMENT(changeValue);

	davisCommonState cstate = userData;

	if (event == ATTRIBUTE_MODIFIED) {
		// Search through all biases for a matching one and send it out.
		for (size_t i = 0; i < BIAS_MAX_NUM_DESC; i++) {
			if (cstate->chipBiases[i] == NULL) {
				// Reached end of valid biases.
				break;
			}

			if (str_equals(sshsNodeGetName(node), cstate->chipBiases[i]->name)) {
				// Found it, send it.
				spiConfigSend(cstate->deviceHandle, FPGA_CHIPBIAS, cstate->chipBiases[i]->address,
					(*cstate->chipBiases[i]->generatorFunction)(sshsNodeGetParent(node), cstate->chipBiases[i]->name));
				break;
			}
		}
	}
}

static void sendBiases(sshsNode moduleNode, davisCommonState cstate) {
	sshsNode biasNode = sshsGetRelativeNode(moduleNode, "bias/");

	// Go through all the biases and send them all out.
	for (size_t i = 0; i < BIAS_MAX_NUM_DESC; i++) {
		if (cstate->chipBiases[i] == NULL) {
			// Reached end of valid biases.
			break;
		}

		spiConfigSend(cstate->deviceHandle, FPGA_CHIPBIAS, cstate->chipBiases[i]->address,
			(*cstate->chipBiases[i]->generatorFunction)(biasNode, cstate->chipBiases[i]->name));
	}
}

static void ChipSRListener(sshsNode node, void *userData, enum sshs_node_attribute_events event, const char *changeKey,
	enum sshs_node_attr_value_type changeType, union sshs_node_attr_value changeValue) {
	davisCommonState cstate = userData;

	if (event == ATTRIBUTE_MODIFIED) {
		if (str_equals(sshsNodeGetName(node), "aps")) {
			if (changeType == BOOL && str_equals(changeKey, "GlobalShutter")) {
				spiConfigSend(cstate->deviceHandle, FPGA_CHIPBIAS, 142, changeValue.boolean);
			}
		}
		else {
			// If not called from 'aps' node, must be 'chip' node.
			// Search through all config-chain settings for a matching one and send it out.
			for (size_t i = 0; i < CONFIGCHAIN_MAX_NUM_DESC; i++) {
				if (cstate->chipConfigChain[i] == NULL) {
					// Reached end of valid config-chain settings.
					break;
				}

				if (str_equals(changeKey, cstate->chipConfigChain[i]->name)) {
					// Found it, send it.
					if (cstate->chipConfigChain[i]->type == BYTE) {
						spiConfigSend(cstate->deviceHandle, FPGA_CHIPBIAS, cstate->chipConfigChain[i]->address,
							changeValue.ubyte);
					}
					else {
						spiConfigSend(cstate->deviceHandle, FPGA_CHIPBIAS, cstate->chipConfigChain[i]->address,
							changeValue.boolean);
					}
					break;
				}
			}
		}
	}
}

static void sendChipSR(sshsNode moduleNode, davisCommonState cstate) {
	sshsNode chipNode = sshsGetRelativeNode(moduleNode, "chip/");
	sshsNode apsNode = sshsGetRelativeNode(moduleNode, "aps/");

	// Go through all the config-chain settings and send them all out.
	for (size_t i = 0; i < CONFIGCHAIN_MAX_NUM_DESC; i++) {
		if (cstate->chipConfigChain[i] == NULL) {
			// Reached end of valid config-chain settings.
			break;
		}

		// Either boolean or byte-wise config-chain settings.
		if (cstate->chipConfigChain[i]->type == BYTE) {
			spiConfigSend(cstate->deviceHandle, FPGA_CHIPBIAS, cstate->chipConfigChain[i]->address,
				sshsNodeGetByte(chipNode, cstate->chipConfigChain[i]->name));
		}
		else {
			spiConfigSend(cstate->deviceHandle, FPGA_CHIPBIAS, cstate->chipConfigChain[i]->address,
				sshsNodeGetBool(chipNode, cstate->chipConfigChain[i]->name));
		}
	}

	// The GlobalShutter setting is sent separately, as it resides
	// in another configuration node (the APS one) to avoid duplication.
	// GS may not exist on chips that don't have it.
	if (sshsNodeAttrExists(apsNode, "GlobalShutter", BOOL)) {
		spiConfigSend(cstate->deviceHandle, FPGA_CHIPBIAS, 142, sshsNodeGetBool(apsNode, "GlobalShutter"));
	}
}
