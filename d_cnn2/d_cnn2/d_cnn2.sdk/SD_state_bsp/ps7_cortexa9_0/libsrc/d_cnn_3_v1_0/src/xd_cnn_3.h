// ==============================================================
// File generated on Wed Dec 17 16:39:23 +0800 2025
// Vivado(TM) HLS - High-Level Synthesis from C, C++ and SystemC v2018.3 (64-bit)
// SW Build 2405991 on Thu Dec  6 23:38:27 MST 2018
// IP Build 2404404 on Fri Dec  7 01:43:56 MST 2018
// Copyright 1986-2018 Xilinx, Inc. All Rights Reserved.
// ==============================================================
#ifndef XD_CNN_3_H
#define XD_CNN_3_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/
#ifndef __linux__
#include "xil_types.h"
#include "xil_assert.h"
#include "xstatus.h"
#include "xil_io.h"
#else
#include <stdint.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>
#endif
#include "xd_cnn_3_hw.h"

/**************************** Type Definitions ******************************/
#ifdef __linux__
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#else
typedef struct {
    u16 DeviceId;
    u32 Axilites_BaseAddress;
} XD_cnn_3_Config;
#endif

typedef struct {
    u32 Axilites_BaseAddress;
    u32 IsReady;
} XD_cnn_3;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __linux__
#define XD_cnn_3_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XD_cnn_3_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XD_cnn_3_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XD_cnn_3_ReadReg(BaseAddress, RegOffset) \
    *(volatile u32*)((BaseAddress) + (RegOffset))

#define Xil_AssertVoid(expr)    assert(expr)
#define Xil_AssertNonvoid(expr) assert(expr)

#define XST_SUCCESS             0
#define XST_DEVICE_NOT_FOUND    2
#define XST_OPEN_DEVICE_FAILED  3
#define XIL_COMPONENT_IS_READY  1
#endif

/************************** Function Prototypes *****************************/
#ifndef __linux__
int XD_cnn_3_Initialize(XD_cnn_3 *InstancePtr, u16 DeviceId);
XD_cnn_3_Config* XD_cnn_3_LookupConfig(u16 DeviceId);
int XD_cnn_3_CfgInitialize(XD_cnn_3 *InstancePtr, XD_cnn_3_Config *ConfigPtr);
#else
int XD_cnn_3_Initialize(XD_cnn_3 *InstancePtr, const char* InstanceName);
int XD_cnn_3_Release(XD_cnn_3 *InstancePtr);
#endif

void XD_cnn_3_Start(XD_cnn_3 *InstancePtr);
u32 XD_cnn_3_IsDone(XD_cnn_3 *InstancePtr);
u32 XD_cnn_3_IsIdle(XD_cnn_3 *InstancePtr);
u32 XD_cnn_3_IsReady(XD_cnn_3 *InstancePtr);
void XD_cnn_3_EnableAutoRestart(XD_cnn_3 *InstancePtr);
void XD_cnn_3_DisableAutoRestart(XD_cnn_3 *InstancePtr);

u32 XD_cnn_3_Get_result(XD_cnn_3 *InstancePtr);
u32 XD_cnn_3_Get_result_vld(XD_cnn_3 *InstancePtr);

void XD_cnn_3_InterruptGlobalEnable(XD_cnn_3 *InstancePtr);
void XD_cnn_3_InterruptGlobalDisable(XD_cnn_3 *InstancePtr);
void XD_cnn_3_InterruptEnable(XD_cnn_3 *InstancePtr, u32 Mask);
void XD_cnn_3_InterruptDisable(XD_cnn_3 *InstancePtr, u32 Mask);
void XD_cnn_3_InterruptClear(XD_cnn_3 *InstancePtr, u32 Mask);
u32 XD_cnn_3_InterruptGetEnabled(XD_cnn_3 *InstancePtr);
u32 XD_cnn_3_InterruptGetStatus(XD_cnn_3 *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
