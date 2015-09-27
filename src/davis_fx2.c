#include "davis_fx2.h"

caerDeviceHandle davisFX2Open(uint16_t deviceID, uint8_t busNumberRestrict, uint8_t devAddressRestrict,
	const char *serialNumberRestrict) {
	caerLog(CAER_LOG_DEBUG, __func__, "Initializing " DEVICE_NAME ".");

	davisFX2Handle handle = calloc(1, sizeof(*handle));
	if (handle == NULL) {
		// Failed to allocate memory for device handle!
		caerLog(CAER_LOG_CRITICAL, __func__, "Failed to allocate memory for device handle.");
		return (NULL);
	}

	bool openRetVal = davisCommonOpen((davisHandle) handle, DEVICE_VID, DEVICE_PID, DEVICE_DID_TYPE, DEVICE_NAME,
		deviceID, busNumberRestrict, devAddressRestrict, serialNumberRestrict, REQUIRED_LOGIC_REVISION,
		REQUIRED_FIRMWARE_VERSION);
	if (!openRetVal) {
		free(handle);

		// Failed to open device and grab basic information!
		caerLog(CAER_LOG_CRITICAL, __func__, "Failed to open device.");
		return (NULL);
	}

	// FX2 specific configuration.
	davisFX2ConfigSet((caerDeviceHandle) handle, DAVIS_CONFIG_DVS, DAVIS_CONFIG_DVS_ACK_DELAY_ROW, 14);
	davisFX2ConfigSet((caerDeviceHandle) handle, DAVIS_CONFIG_DVS, DAVIS_CONFIG_DVS_ACK_EXTENSION_ROW, 1);

	return ((caerDeviceHandle) handle);
}

bool davisFX2Close(caerDeviceHandle cdh) {
	return (davisCommonClose((davisHandle) cdh));
}

bool davisFX2SendDefaultConfig(caerDeviceHandle cdh) {
	davisHandle handle = (davisHandle) cdh;

}

bool davisFX2ConfigSet(caerDeviceHandle cdh, int8_t modAddr, uint8_t paramAddr, uint32_t param) {
	davisHandle handle = (davisHandle) cdh;

}

bool davisFX2ConfigGet(caerDeviceHandle cdh, int8_t modAddr, uint8_t paramAddr, uint32_t *param) {
	davisHandle handle = (davisHandle) cdh;

}

static void sendBias(libusb_device_handle *devHandle, uint8_t biasAddress, uint16_t biasValue);
static void sendBiases(sshsNode moduleNode, davisCommonState cstate);
static void sendChipSR(sshsNode moduleNode, davisCommonState cstate);
static void BiasesListener(sshsNode node, void *userData, enum sshs_node_attribute_events event, const char *changeKey,
	enum sshs_node_attr_value_type changeType, union sshs_node_attr_value changeValue);
static void ChipSRListener(sshsNode node, void *userData, enum sshs_node_attribute_events event, const char *changeKey,
	enum sshs_node_attr_value_type changeType, union sshs_node_attr_value changeValue);

static void sendBias(libusb_device_handle *devHandle, uint8_t biasAddress, uint16_t biasValue) {
	// All biases are two byte quantities.
	uint8_t bias[2];

	// Put the value in.
	bias[0] = U8T(biasValue >> 8);
	bias[1] = U8T(biasValue >> 0);

	libusb_control_transfer(devHandle, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
		VR_CHIP_BIAS, biasAddress, 0, bias, sizeof(bias), 0);
}

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
				sendBias(cstate->deviceHandle, cstate->chipBiases[i]->address,
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

		sendBias(cstate->deviceHandle, cstate->chipBiases[i]->address,
			(*cstate->chipBiases[i]->generatorFunction)(biasNode, cstate->chipBiases[i]->name));
	}
}

static void ChipSRListener(sshsNode node, void *userData, enum sshs_node_attribute_events event, const char *changeKey,
	enum sshs_node_attr_value_type changeType, union sshs_node_attr_value changeValue) {
	UNUSED_ARGUMENT(changeValue);

	caerModuleData moduleData = userData;
	sshsNode moduleNode = moduleData->moduleNode;
	davisCommonState cstate = &((davisFX2State) moduleData->moduleState)->cstate;

	if (event == ATTRIBUTE_MODIFIED) {
		if (str_equals(sshsNodeGetName(node), "aps")) {
			if (changeType == BOOL && str_equals(changeKey, "GlobalShutter")) {
				sendChipSR(moduleNode, cstate);
			}
		}
		else {
			// If not called from 'aps' node, must be 'chip' node, so we
			// always send the chip configuration chain in that case.
			sendChipSR(moduleNode, cstate);
		}
	}
}

static void sendChipSR(sshsNode moduleNode, davisCommonState cstate) {
	// Only DAVIS240 can be used with the FX2 boards.
	// This generates the full shift register content manually, as the single
	// configuration options are not addressable like with FX3 boards.
	sshsNode chipNode = sshsGetRelativeNode(moduleNode, "chip/");
	sshsNode apsNode = sshsGetRelativeNode(moduleNode, "aps/");

	// A total of 56 bits (7 bytes) of configuration.
	uint8_t chipSR[7] = { 0 };

	// Debug muxes control.
	chipSR[0] |= U8T((sshsNodeGetByte(chipNode, "DigitalMux3") & 0x0F) << 4);
	chipSR[0] |= U8T((sshsNodeGetByte(chipNode, "DigitalMux2") & 0x0F) << 0);
	chipSR[1] |= U8T((sshsNodeGetByte(chipNode, "DigitalMux1") & 0x0F) << 4);
	chipSR[1] |= U8T((sshsNodeGetByte(chipNode, "DigitalMux0") & 0x0F) << 0);

	chipSR[5] |= U8T((sshsNodeGetByte(chipNode, "AnalogMux2") & 0x0F) << 4);
	chipSR[5] |= U8T((sshsNodeGetByte(chipNode, "AnalogMux1") & 0x0F) << 0);
	chipSR[6] |= U8T((sshsNodeGetByte(chipNode, "AnalogMux0") & 0x0F) << 4);

	chipSR[6] |= U8T((sshsNodeGetByte(chipNode, "BiasMux0") & 0x0F) << 0);

	// Bytes 2-4 contain the actual 24 configuration bits. 17 are unused.
	// GS may not exist on chips that don't have it.
	if (sshsNodeAttrExists(apsNode, "GlobalShutter", BOOL)) {
		bool globalShutter = sshsNodeGetBool(apsNode, "GlobalShutter");
		if (globalShutter) {
			// Flip bit on if enabled.
			chipSR[4] |= (1 << 6);
		}
	}

	bool useAOut = sshsNodeGetBool(chipNode, "UseAOut");
	if (useAOut) {
		// Flip bit on if enabled.
		chipSR[4] |= (1 << 5);
	}

	bool AERnArow = sshsNodeGetBool(chipNode, "AERnArow");
	if (AERnArow) {
		// Flip bit on if enabled.
		chipSR[4] |= (1 << 4);
	}

	// Only DAVIS240 A/B have this, C doesn't.
	if (sshsNodeAttrExists(chipNode, "SpecialPixelControl", BOOL)) {
		bool specialPixelControl = sshsNodeGetBool(chipNode, "SpecialPixelControl");
		if (specialPixelControl) {
			// Flip bit on if enabled.
			chipSR[4] |= (1 << 3);
		}
	}

	bool resetTestpixel = sshsNodeGetBool(chipNode, "ResetTestPixel");
	if (resetTestpixel) {
		// Flip bit on if enabled.
		chipSR[4] |= (1 << 2);
	}

	bool typeNCalib = sshsNodeGetBool(chipNode, "TypeNCalibNeuron");
	if (typeNCalib) {
		// Flip bit on if enabled.
		chipSR[4] |= (1 << 1);
	}

	bool resetCalib = sshsNodeGetBool(chipNode, "ResetCalibNeuron");
	if (resetCalib) {
		// Flip bit on if enabled.
		chipSR[4] |= (1 << 0);
	}

	libusb_control_transfer(cstate->deviceHandle,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, VR_CHIP_DIAG, 0, 0, chipSR,
		sizeof(chipSR), 0);
}
