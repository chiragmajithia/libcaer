#include "davis_fx2.h"

static bool sendBias(libusb_device_handle *devHandle, uint8_t biasAddress, uint16_t biasValue);
static bool receiveBias(libusb_device_handle *devHandle, uint8_t biasAddress, uint16_t *biasValue);
static bool sendChipSR(libusb_device_handle *devHandle, uint8_t paramAddr, uint8_t param);
static bool receiveChipSR(libusb_device_handle *devHandle, uint8_t paramAddr, uint8_t *param);

caerDeviceHandle davisFX2Open(uint16_t deviceID, uint8_t busNumberRestrict, uint8_t devAddressRestrict,
	const char *serialNumberRestrict) {
	caerLog(CAER_LOG_DEBUG, __func__, "Initializing %s.", DAVIS_FX2_DEVICE_NAME);

	davisFX2Handle handle = calloc(1, sizeof(*handle));
	if (handle == NULL) {
		// Failed to allocate memory for device handle!
		caerLog(CAER_LOG_CRITICAL, __func__, "Failed to allocate memory for device handle.");
		return (NULL);
	}

	bool openRetVal = davisCommonOpen((davisHandle) handle, DAVIS_FX2_DEVICE_VID, DAVIS_FX2_DEVICE_PID,
	DAVIS_FX2_DEVICE_DID_TYPE, DAVIS_FX2_DEVICE_NAME, deviceID, busNumberRestrict, devAddressRestrict,
		serialNumberRestrict, DAVIS_FX2_REQUIRED_LOGIC_REVISION,
		DAVIS_FX2_REQUIRED_FIRMWARE_VERSION);
	if (!openRetVal) {
		free(handle);

		// Failed to open device and grab basic information!
		caerLog(CAER_LOG_CRITICAL, __func__, "Failed to open device.");
		return (NULL);
	}

	return ((caerDeviceHandle) handle);
}

bool davisFX2Close(caerDeviceHandle cdh) {
	return (davisCommonClose((davisHandle) cdh));
}

bool davisFX2SendDefaultConfig(caerDeviceHandle cdh) {
	davisHandle handle = (davisHandle) cdh;

	// First send default chip/bias config.
	// We send the chip/bias configuration separately here for FX2.
	// FX2 has a different mechanism there, and only supports DAVIS240 chips!
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_DIFFBN,
		caerBiasGenerateCoarseFine(4, 39, true, true, true, true));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_ONBN,
		caerBiasGenerateCoarseFine(5, 255, true, true, true, true));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_OFFBN,
		caerBiasGenerateCoarseFine(4, 0, true, true, true, true));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_APSCASEPC,
		caerBiasGenerateCoarseFine(5, 185, true, true, false, true));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_DIFFCASBNC,
		caerBiasGenerateCoarseFine(5, 115, true, true, false, true));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_APSROSFBN,
		caerBiasGenerateCoarseFine(6, 219, true, true, true, true));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_LOCALBUFBN,
		caerBiasGenerateCoarseFine(5, 164, true, true, true, true));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PIXINVBN,
		caerBiasGenerateCoarseFine(5, 129, true, true, true, true));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRBP,
		caerBiasGenerateCoarseFine(2, 58, true, false, true, true));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRSFBP,
		caerBiasGenerateCoarseFine(1, 16, true, false, true, true));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_REFRBP,
		caerBiasGenerateCoarseFine(4, 25, true, false, true, true));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_AEPDBN,
		caerBiasGenerateCoarseFine(6, 91, true, true, true, true));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_LCOLTIMEOUTBN,
		caerBiasGenerateCoarseFine(5, 49, true, true, true, true));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_AEPUXBP,
		caerBiasGenerateCoarseFine(4, 80, true, false, true, true));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_AEPUYBP,
		caerBiasGenerateCoarseFine(7, 152, true, false, true, true));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_IFTHRBN,
		caerBiasGenerateCoarseFine(5, 255, true, true, true, true));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_IFREFRBN,
		caerBiasGenerateCoarseFine(5, 255, true, true, true, true));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PADFOLLBN,
		caerBiasGenerateCoarseFine(7, 215, true, true, true, true));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_APSOVERFLOWLEVEL,
		caerBiasGenerateCoarseFine(6, 253, true, true, true, true));

	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_BIASBUFFER,
		caerBiasGenerateCoarseFine(5, 254, true, true, true, true));

	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_SSP,
		caerBiasGenerateShiftedSource(33, 20, SHIFTED_SOURCE, SPLIT_GATE));
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_SSN,
		caerBiasGenerateShiftedSource(33, 21, SHIFTED_SOURCE, SPLIT_GATE));

	davisFX2ConfigSet(cdh, DAVIS_CONFIG_CHIP, DAVIS240_CONFIG_CHIP_DIGITALMUX0, 0);
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_CHIP, DAVIS240_CONFIG_CHIP_DIGITALMUX1, 0);
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_CHIP, DAVIS240_CONFIG_CHIP_DIGITALMUX2, 0);
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_CHIP, DAVIS240_CONFIG_CHIP_DIGITALMUX3, 0);
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_CHIP, DAVIS240_CONFIG_CHIP_ANALOGMUX0, 0);
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_CHIP, DAVIS240_CONFIG_CHIP_ANALOGMUX1, 0);
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_CHIP, DAVIS240_CONFIG_CHIP_ANALOGMUX2, 0);
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_CHIP, DAVIS240_CONFIG_CHIP_BIASMUX0, 0);
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_CHIP, DAVIS240_CONFIG_CHIP_RESETCALIBNEURON, true);
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_CHIP, DAVIS240_CONFIG_CHIP_TYPENCALIBNEURON, false);
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_CHIP, DAVIS240_CONFIG_CHIP_RESETTESTPIXEL, true);
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_CHIP, DAVIS240_CONFIG_CHIP_SPECIALPIXELCONTROL, false);
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_CHIP, DAVIS240_CONFIG_CHIP_AERNAROW, false);
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_CHIP, DAVIS240_CONFIG_CHIP_USEAOUT, false);
	davisFX2ConfigSet(cdh, DAVIS_CONFIG_CHIP, DAVIS240_CONFIG_CHIP_GLOBALSHUTTER, handle->info.apsHasGlobalShutter);

	// Send default FPGA config.
	if (!davisCommonSendDefaultFPGAConfig(handle)) {
		return (false);
	}

	// FX2 specific FPGA configuration.
	if (!davisFX2ConfigSet(cdh, DAVIS_CONFIG_DVS, DAVIS_CONFIG_DVS_ACK_DELAY_ROW, 14)) {
		return (false);
	}

	if (!davisFX2ConfigSet(cdh, DAVIS_CONFIG_DVS, DAVIS_CONFIG_DVS_ACK_EXTENSION_ROW, 4)) {
		return (false);
	}

	return (true);
}

bool davisFX2ConfigSet(caerDeviceHandle cdh, int8_t modAddr, uint8_t paramAddr, uint32_t param) {
	davisHandle handle = (davisHandle) cdh;

	if (modAddr == DAVIS_CONFIG_BIAS) {
		// Biasing is done differently for FX2 cameras, via separate vendor request.
		return (sendBias(handle->state.deviceHandle, paramAddr, U16T(param)));
	}

	if (modAddr == DAVIS_CONFIG_CHIP) {
		// Chip configuration is done differently for FX2 cameras, via separate vendor request.
		return (sendChipSR(handle->state.deviceHandle, paramAddr, U8T(param)));
	}

	return (davisCommonConfigSet(handle, modAddr, paramAddr, param));
}

bool davisFX2ConfigGet(caerDeviceHandle cdh, int8_t modAddr, uint8_t paramAddr, uint32_t *param) {
	davisHandle handle = (davisHandle) cdh;

	if (modAddr == DAVIS_CONFIG_BIAS) {
		// Biasing is done differently for FX2 cameras, via separate vendor request.
		uint16_t param16 = U16T(*param);
		return (receiveBias(handle->state.deviceHandle, paramAddr, &param16));
	}

	if (modAddr == DAVIS_CONFIG_CHIP) {
		// Chip configuration is done differently for FX2 cameras, via separate vendor request.
		uint8_t param8 = U8T(*param);
		return (receiveChipSR(handle->state.deviceHandle, paramAddr, &param8));
	}

	return (davisCommonConfigGet(handle, modAddr, paramAddr, param));
}

static bool sendBias(libusb_device_handle *devHandle, uint8_t biasAddress, uint16_t biasValue) {
	// Check address validity.
	if (biasAddress >= 22) {
		return (false);
	}

	// All biases are two byte quantities.
	uint8_t bias[2] = { 0 };

	// Put the value in.
	bias[0] = U8T(biasValue >> 8);
	bias[1] = U8T(biasValue >> 0);

	return (libusb_control_transfer(devHandle,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
		VENDOR_REQUEST_CHIP_BIAS, biasAddress, 0, bias, sizeof(bias), 0) == LIBUSB_SUCCESS);
}

static bool receiveBias(libusb_device_handle *devHandle, uint8_t biasAddress, uint16_t *biasValue) {
	// Check address validity.
	if (biasAddress >= 22) {
		return (false);
	}

	// All biases are two byte quantities.
	uint8_t bias[2] = { 0 };

	if (libusb_control_transfer(devHandle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
	VENDOR_REQUEST_CHIP_BIAS, biasAddress, 0, bias, sizeof(bias), 0) != LIBUSB_SUCCESS) {
		return (false);
	}

	*biasValue = U16T(bias[1] << 0);
	*biasValue |= U16T(bias[0] << 8);

	return (true);
}

static bool sendChipSR(libusb_device_handle *devHandle, uint8_t paramAddr, uint8_t param) {
	// Only DAVIS240 can be used with the FX2 boards.
	// This generates the full shift register content manually, as the single
	// configuration options are not addressable like with FX3 boards.
	// A total of 56 bits (7 bytes) of configuration.
	uint8_t chipSR[7] = { 0 };

	// Get the current configuration from the device.
	if (libusb_control_transfer(devHandle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
	VENDOR_REQUEST_CHIP_DIAG, 0, 0, chipSR, sizeof(chipSR), 0) != LIBUSB_SUCCESS) {
		return (false);
	}

	// Now update the correct configuration value.
	switch (paramAddr) {
		case DAVIS240_CONFIG_CHIP_DIGITALMUX0: {
			uint8_t digMux0 = U8T(param & 0x0F);
			uint8_t digMux1 = U8T(chipSR[1] & 0xF0);
			chipSR[1] = U8T(digMux0 | digMux1);
			break;
		}

		case DAVIS240_CONFIG_CHIP_DIGITALMUX1: {
			uint8_t digMux0 = U8T(chipSR[1] & 0x0F);
			uint8_t digMux1 = U8T((param << 4) & 0xF0);
			chipSR[1] = U8T(digMux0 | digMux1);
			break;
		}

		case DAVIS240_CONFIG_CHIP_DIGITALMUX2: {
			uint8_t digMux2 = U8T(param & 0x0F);
			uint8_t digMux3 = U8T(chipSR[0] & 0xF0);
			chipSR[0] = U8T(digMux2 | digMux3);
			break;
		}

		case DAVIS240_CONFIG_CHIP_DIGITALMUX3: {
			uint8_t digMux2 = U8T(chipSR[0] & 0x0F);
			uint8_t digMux3 = U8T((param << 4) & 0xF0);
			chipSR[0] = U8T(digMux2 | digMux3);
			break;
		}

		case DAVIS240_CONFIG_CHIP_ANALOGMUX0: {
			uint8_t biasMux0 = U8T(chipSR[6] & 0x0F);
			uint8_t anaMux0 = U8T((param << 4) & 0xF0);
			chipSR[6] = U8T(biasMux0 | anaMux0);
			break;
		}

		case DAVIS240_CONFIG_CHIP_ANALOGMUX1: {
			uint8_t anaMux1 = U8T(param & 0x0F);
			uint8_t anaMux2 = U8T(chipSR[5] & 0xF0);
			chipSR[5] = U8T(anaMux1 | anaMux2);
			break;
		}

		case DAVIS240_CONFIG_CHIP_ANALOGMUX2: {
			uint8_t anaMux1 = U8T(chipSR[5] & 0x0F);
			uint8_t anaMux2 = U8T((param << 4) & 0xF0);
			chipSR[5] = U8T(anaMux1 | anaMux2);
			break;
		}

		case DAVIS240_CONFIG_CHIP_BIASMUX0: {
			uint8_t biasMux0 = U8T(param & 0x0F);
			uint8_t anaMux0 = U8T(chipSR[6] & 0xF0);
			chipSR[6] = U8T(biasMux0 | anaMux0);
			break;
		}
		case DAVIS240_CONFIG_CHIP_RESETCALIBNEURON:
			if (param) {
				// Flip bit on if enabled.
				chipSR[4] |= U8T(1 << 0);
			}
			else {
				// Flip bit off if disabled.
				chipSR[4] &= U8T(~(1 << 0));
			}
			break;

		case DAVIS240_CONFIG_CHIP_TYPENCALIBNEURON:
			if (param) {
				// Flip bit on if enabled.
				chipSR[4] |= U8T(1 << 1);
			}
			else {
				// Flip bit off if disabled.
				chipSR[4] &= U8T(~(1 << 1));
			}
			break;

		case DAVIS240_CONFIG_CHIP_RESETTESTPIXEL:
			if (param) {
				// Flip bit on if enabled.
				chipSR[4] |= U8T(1 << 2);
			}
			else {
				// Flip bit off if disabled.
				chipSR[4] &= U8T(~(1 << 2));
			}
			break;

		case DAVIS240_CONFIG_CHIP_SPECIALPIXELCONTROL:
			if (param) {
				// Flip bit on if enabled.
				chipSR[4] |= U8T(1 << 3);
			}
			else {
				// Flip bit off if disabled.
				chipSR[4] &= U8T(~(1 << 3));
			}
			break;

		case DAVIS240_CONFIG_CHIP_AERNAROW:
			if (param) {
				// Flip bit on if enabled.
				chipSR[4] |= U8T(1 << 4);
			}
			else {
				// Flip bit off if disabled.
				chipSR[4] &= U8T(~(1 << 4));
			}
			break;

		case DAVIS240_CONFIG_CHIP_USEAOUT:
			if (param) {
				// Flip bit on if enabled.
				chipSR[4] |= U8T(1 << 5);
			}
			else {
				// Flip bit off if disabled.
				chipSR[4] &= U8T(~(1 << 5));
			}
			break;

		case DAVIS240_CONFIG_CHIP_GLOBALSHUTTER:
			if (param) {
				// Flip bit on if enabled.
				chipSR[4] |= U8T(1 << 6);
			}
			else {
				// Flip bit off if disabled.
				chipSR[4] &= U8T(~(1 << 6));
			}
			break;

		default:
			return (false);
			break;
	}

	// Send updated configuration back to device.
	return (libusb_control_transfer(devHandle,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
		VENDOR_REQUEST_CHIP_DIAG, 0, 0, chipSR, sizeof(chipSR), 0) == LIBUSB_SUCCESS);
}

static bool receiveChipSR(libusb_device_handle *devHandle, uint8_t paramAddr, uint8_t *param) {
	// A total of 56 bits (7 bytes) of configuration.
	uint8_t chipSR[7] = { 0 };

	// Get the current configuration from the device.
	if (libusb_control_transfer(devHandle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
	VENDOR_REQUEST_CHIP_DIAG, 0, 0, chipSR, sizeof(chipSR), 0) != LIBUSB_SUCCESS) {
		return (false);
	}

	// Parse the returned configuration bytes and return the value we're interested in.
	switch (paramAddr) {
		case DAVIS240_CONFIG_CHIP_DIGITALMUX0:
			*param = U8T(chipSR[1] & 0x0F);
			break;

		case DAVIS240_CONFIG_CHIP_DIGITALMUX1:
			*param = U8T((chipSR[1] >> 4) & 0x0F);
			break;

		case DAVIS240_CONFIG_CHIP_DIGITALMUX2:
			*param = U8T(chipSR[0] & 0x0F);
			break;

		case DAVIS240_CONFIG_CHIP_DIGITALMUX3:
			*param = U8T((chipSR[0] >> 4) & 0x0F);
			break;

		case DAVIS240_CONFIG_CHIP_ANALOGMUX0:
			*param = U8T((chipSR[6] >> 4) & 0x0F);
			break;

		case DAVIS240_CONFIG_CHIP_ANALOGMUX1:
			*param = U8T(chipSR[5] & 0x0F);
			break;

		case DAVIS240_CONFIG_CHIP_ANALOGMUX2:
			*param = U8T((chipSR[5] >> 4) & 0x0F);
			break;

		case DAVIS240_CONFIG_CHIP_BIASMUX0:
			*param = U8T(chipSR[6] & 0x0F);
			break;

		case DAVIS240_CONFIG_CHIP_RESETCALIBNEURON:
			*param = U8T((chipSR[4] >> 0) & 0x01);
			break;

		case DAVIS240_CONFIG_CHIP_TYPENCALIBNEURON:
			*param = U8T((chipSR[4] >> 1) & 0x01);
			break;

		case DAVIS240_CONFIG_CHIP_RESETTESTPIXEL:
			*param = U8T((chipSR[4] >> 2) & 0x01);
			break;

		case DAVIS240_CONFIG_CHIP_SPECIALPIXELCONTROL:
			*param = U8T((chipSR[4] >> 3) & 0x01);
			break;

		case DAVIS240_CONFIG_CHIP_AERNAROW:
			*param = U8T((chipSR[4] >> 4) & 0x01);
			break;

		case DAVIS240_CONFIG_CHIP_USEAOUT:
			*param = U8T((chipSR[4] >> 5) & 0x01);
			break;

		case DAVIS240_CONFIG_CHIP_GLOBALSHUTTER:
			*param = U8T((chipSR[4] >> 6) & 0x01);
			break;

		default:
			return (false);
			break;
	}

	return (true);
}
