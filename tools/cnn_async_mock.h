#ifndef CNN_ASYNC_MOCK_H
#define CNN_ASYNC_MOCK_H
#include <stdint.h>
#include "xil_types.h"
typedef u64 XTime;
typedef struct { int dummy; } XAxiDma;
typedef struct { int dummy; } XAxiDma_Config;
#include "xparameters.h"
#define XST_SUCCESS 0
#define XST_FAILURE 1
#define XAXIDMA_DMA_TO_DEVICE 0
#define COUNTS_PER_SECOND 1000000ULL
void XTime_GetTime(XTime *t);
void Xil_Out32(UINTPTR addr,u32 value);
u32 Xil_In32(UINTPTR addr);
void Xil_DCacheFlushRange(UINTPTR addr,u32 length);
void xil_printf(const char *format,...);
XAxiDma_Config *XAxiDma_LookupConfig(u32 id);
int XAxiDma_CfgInitialize(XAxiDma *dma,XAxiDma_Config *cfg);
int XAxiDma_HasSg(XAxiDma *dma);
void XAxiDma_Reset(XAxiDma *dma);
int XAxiDma_ResetIsDone(XAxiDma *dma);
int XAxiDma_Busy(XAxiDma *dma,int dir);
int XAxiDma_SimpleTransfer(XAxiDma *dma,UINTPTR address,u32 length,int dir);
#endif
