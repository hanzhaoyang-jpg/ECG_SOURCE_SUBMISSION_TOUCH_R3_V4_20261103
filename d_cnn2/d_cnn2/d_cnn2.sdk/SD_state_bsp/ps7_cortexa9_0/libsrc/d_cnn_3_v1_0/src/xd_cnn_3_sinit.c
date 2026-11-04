// ==============================================================
// File generated on Wed Dec 17 16:39:23 +0800 2025
// Vivado(TM) HLS - High-Level Synthesis from C, C++ and SystemC v2018.3 (64-bit)
// SW Build 2405991 on Thu Dec  6 23:38:27 MST 2018
// IP Build 2404404 on Fri Dec  7 01:43:56 MST 2018
// Copyright 1986-2018 Xilinx, Inc. All Rights Reserved.
// ==============================================================
#ifndef __linux__

#include "xstatus.h"
#include "xparameters.h"
#include "xd_cnn_3.h"

extern XD_cnn_3_Config XD_cnn_3_ConfigTable[];

XD_cnn_3_Config *XD_cnn_3_LookupConfig(u16 DeviceId) {
	XD_cnn_3_Config *ConfigPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_XD_CNN_3_NUM_INSTANCES; Index++) {
		if (XD_cnn_3_ConfigTable[Index].DeviceId == DeviceId) {
			ConfigPtr = &XD_cnn_3_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XD_cnn_3_Initialize(XD_cnn_3 *InstancePtr, u16 DeviceId) {
	XD_cnn_3_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XD_cnn_3_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XD_cnn_3_CfgInitialize(InstancePtr, ConfigPtr);
}

#endif

