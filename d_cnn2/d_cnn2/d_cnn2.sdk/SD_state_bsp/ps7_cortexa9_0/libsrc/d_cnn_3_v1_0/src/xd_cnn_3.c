// ==============================================================
// File generated on Wed Dec 17 16:39:23 +0800 2025
// Vivado(TM) HLS - High-Level Synthesis from C, C++ and SystemC v2018.3 (64-bit)
// SW Build 2405991 on Thu Dec  6 23:38:27 MST 2018
// IP Build 2404404 on Fri Dec  7 01:43:56 MST 2018
// Copyright 1986-2018 Xilinx, Inc. All Rights Reserved.
// ==============================================================
/***************************** Include Files *********************************/
#include "xd_cnn_3.h"

/************************** Function Implementation *************************/
#ifndef __linux__
int XD_cnn_3_CfgInitialize(XD_cnn_3 *InstancePtr, XD_cnn_3_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Axilites_BaseAddress = ConfigPtr->Axilites_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XD_cnn_3_Start(XD_cnn_3 *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XD_cnn_3_ReadReg(InstancePtr->Axilites_BaseAddress, XD_CNN_3_AXILITES_ADDR_AP_CTRL) & 0x80;
    XD_cnn_3_WriteReg(InstancePtr->Axilites_BaseAddress, XD_CNN_3_AXILITES_ADDR_AP_CTRL, Data | 0x01);
}

u32 XD_cnn_3_IsDone(XD_cnn_3 *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XD_cnn_3_ReadReg(InstancePtr->Axilites_BaseAddress, XD_CNN_3_AXILITES_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XD_cnn_3_IsIdle(XD_cnn_3 *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XD_cnn_3_ReadReg(InstancePtr->Axilites_BaseAddress, XD_CNN_3_AXILITES_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XD_cnn_3_IsReady(XD_cnn_3 *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XD_cnn_3_ReadReg(InstancePtr->Axilites_BaseAddress, XD_CNN_3_AXILITES_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XD_cnn_3_EnableAutoRestart(XD_cnn_3 *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XD_cnn_3_WriteReg(InstancePtr->Axilites_BaseAddress, XD_CNN_3_AXILITES_ADDR_AP_CTRL, 0x80);
}

void XD_cnn_3_DisableAutoRestart(XD_cnn_3 *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XD_cnn_3_WriteReg(InstancePtr->Axilites_BaseAddress, XD_CNN_3_AXILITES_ADDR_AP_CTRL, 0);
}

u32 XD_cnn_3_Get_result(XD_cnn_3 *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XD_cnn_3_ReadReg(InstancePtr->Axilites_BaseAddress, XD_CNN_3_AXILITES_ADDR_RESULT_DATA);
    return Data;
}

u32 XD_cnn_3_Get_result_vld(XD_cnn_3 *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XD_cnn_3_ReadReg(InstancePtr->Axilites_BaseAddress, XD_CNN_3_AXILITES_ADDR_RESULT_CTRL);
    return Data & 0x1;
}

void XD_cnn_3_InterruptGlobalEnable(XD_cnn_3 *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XD_cnn_3_WriteReg(InstancePtr->Axilites_BaseAddress, XD_CNN_3_AXILITES_ADDR_GIE, 1);
}

void XD_cnn_3_InterruptGlobalDisable(XD_cnn_3 *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XD_cnn_3_WriteReg(InstancePtr->Axilites_BaseAddress, XD_CNN_3_AXILITES_ADDR_GIE, 0);
}

void XD_cnn_3_InterruptEnable(XD_cnn_3 *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XD_cnn_3_ReadReg(InstancePtr->Axilites_BaseAddress, XD_CNN_3_AXILITES_ADDR_IER);
    XD_cnn_3_WriteReg(InstancePtr->Axilites_BaseAddress, XD_CNN_3_AXILITES_ADDR_IER, Register | Mask);
}

void XD_cnn_3_InterruptDisable(XD_cnn_3 *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XD_cnn_3_ReadReg(InstancePtr->Axilites_BaseAddress, XD_CNN_3_AXILITES_ADDR_IER);
    XD_cnn_3_WriteReg(InstancePtr->Axilites_BaseAddress, XD_CNN_3_AXILITES_ADDR_IER, Register & (~Mask));
}

void XD_cnn_3_InterruptClear(XD_cnn_3 *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XD_cnn_3_WriteReg(InstancePtr->Axilites_BaseAddress, XD_CNN_3_AXILITES_ADDR_ISR, Mask);
}

u32 XD_cnn_3_InterruptGetEnabled(XD_cnn_3 *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XD_cnn_3_ReadReg(InstancePtr->Axilites_BaseAddress, XD_CNN_3_AXILITES_ADDR_IER);
}

u32 XD_cnn_3_InterruptGetStatus(XD_cnn_3 *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XD_cnn_3_ReadReg(InstancePtr->Axilites_BaseAddress, XD_CNN_3_AXILITES_ADDR_ISR);
}

