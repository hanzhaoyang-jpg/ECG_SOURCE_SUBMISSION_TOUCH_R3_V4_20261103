#include <assert.h>
#include <stdio.h>
#include "cnn_async_mock.h"
#include "ecg_cnn_infer.h"
#include "cn121_ps.h"

static XTime now;
static u32 ctrl,valid,result,starts,reads;
static int busy,fail_transfer;
static UINTPTR dma_address;
static unsigned char saved[ECG_CNN_PACK_LEN];
void XTime_GetTime(XTime *t){*t=now;}
void Xil_Out32(UINTPTR addr,u32 value){assert(addr==XPAR_D_CNN_3_0_S_AXI_AXILITES_BASEADDR && value==1); starts++;}
u32 Xil_In32(UINTPTR addr){u32 v;addr-=XPAR_D_CNN_3_0_S_AXI_AXILITES_BASEADDR;reads++;if(addr==0){v=ctrl;ctrl=0;return v;}if(addr==0x14){v=valid;valid=0;return v;}assert(addr==0x10);return result;}
void Xil_DCacheFlushRange(UINTPTR addr,u32 length){assert(addr && length==864);}
void xil_printf(const char *format,...){(void)format;}
XAxiDma_Config *XAxiDma_LookupConfig(u32 id){static XAxiDma_Config c;(void)id;return &c;}
int XAxiDma_CfgInitialize(XAxiDma *d,XAxiDma_Config *c){(void)d;(void)c;return 0;}
int XAxiDma_HasSg(XAxiDma *d){(void)d;return 0;}
void XAxiDma_Reset(XAxiDma *d){(void)d;busy=0;}
int XAxiDma_ResetIsDone(XAxiDma *d){(void)d;return 1;}
int XAxiDma_Busy(XAxiDma *d,int dir){(void)d;(void)dir;return busy;}
int XAxiDma_SimpleTransfer(XAxiDma *d,UINTPTR addr,u32 len,int dir){(void)d;(void)dir;assert(len==864);dma_address=addr;busy=1;return fail_transfer;}

/* Include implementation to reset simulated power-on state between cases. */
#include "../d_cnn2/d_cnn2/d_cnn2.sdk/SD_state/src/ecg_cnn_infer.c"
int main(void)
{
    float beat[ECG_CNN_FLOAT_LEN];u8 cls;u32 before;unsigned i;
    assert(CN121_CHSET_ECG_DEFAULT==0x0C);
    for(i=0;i<ECG_CNN_FLOAT_LEN;i++)beat[i]=(float)(i%27);
    assert(ecg_cnn_infer_init()==0);
    ctrl=2;valid=1;result=3; /* Stale flags must be drained. */
    assert(ecg_cnn_infer_start(beat,360)==0);
    memcpy(saved,(void*)dma_address,sizeof(saved));
    assert(ecg_cnn_infer_start(beat,360)!=0);
    assert(memcmp(saved,(void*)dma_address,sizeof(saved))==0);
    for(i=0;i<100;i++){
        before=reads;assert(ecg_cnn_infer_poll(&cls)==0);assert(reads-before<=3);
    }
    valid=1;result=2;assert(ecg_cnn_infer_poll(&cls)==0);
    ctrl=2;assert(ecg_cnn_infer_poll(&cls)==0); /* DMA still owns buffer. */
    busy=0;assert(ecg_cnn_infer_poll(&cls)==1 && cls==2);
    assert(ecg_cnn_infer_available());
    assert(ecg_cnn_infer_start(beat,360)==0);
    ctrl=2;busy=0;assert(ecg_cnn_infer_poll(&cls)==0);
    valid=1;result=1;assert(ecg_cnn_infer_poll(&cls)==1 && cls==1);
    assert(ecg_cnn_infer_start(beat,360)==0);
    ctrl=2;valid=1;result=255;busy=0;assert(ecg_cnn_infer_poll(&cls)==-1);
    assert(ecg_cnn_infer_start(beat,360)==0);
    now+=2000000;assert(ecg_cnn_infer_poll(&cls)==-1);
    assert(!ecg_cnn_infer_available());
    assert(ecg_cnn_infer_init()!=0);
    assert(ecg_cnn_infer_start(beat,360)!=0);
    g_cnn_fault=0; /* Simulated board reset for transfer-failure case. */
    fail_transfer=1;
    assert(ecg_cnn_infer_start(beat,360)!=0);
    assert(!ecg_cnn_infer_available());
    assert(starts==5);
    puts("PASS: default 780/4Hz; nonblocking poll; buffer ownership; clear-on-read flags; invalid output; timeout and transfer fault quarantine");
    return 0;
}
