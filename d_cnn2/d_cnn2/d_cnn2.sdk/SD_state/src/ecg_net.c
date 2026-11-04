#include <string.h>

#include "ecg_net.h"
#include "ecg_result.h"
#include "netif/xadapter.h"
#include "lwip/init.h"
#include "lwip/etharp.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xscugic.h"

#define ECG_NET_MAGIC           0x31474345UL
#define ECG_NET_VERSION         1U
#define ECG_NET_TYPE_WAVE       1U
#define ECG_NET_TYPE_EVENT      2U

struct netif server_netif;

typedef struct __attribute__((packed)) {
    u32 magic;
    u16 version;
    u16 type;
    u32 seq;
    u64 sample_index;
    u32 timestamp_ms;
    s16 filtered;
    u8  class_id;
    u8  flags;
} EcgNetWavePacket;

typedef char EcgV1WaveMustBe28Bytes[sizeof(EcgNetWavePacket) == 28U ? 1 : -1];

typedef struct __attribute__((packed)) {
    u32 magic;
    u16 version;
    u16 type;
    u32 seq;
    u32 event_id;
    u64 sample_index;
    u32 timestamp_ms;
    u32 rr_ms;
    u32 hr_bpm;
    u32 abnormal_count;
    u8  class_id;
    u8  confidence;
    u8  pre_seconds;
    u8  post_seconds;
} EcgNetEventPacket;

static struct udp_pcb *g_wave_pcb = 0;
static ip_addr_t g_peer_ip;
static u8 g_ready = 0U;
static u32 g_seq = 0U;
static u32 g_event_id = 0U;
static u32 g_abnormal_count = 0U;

static void ecg_net_setup_exception(void)
{
    Xil_ExceptionInit();
    XScuGic_DeviceInitialize(XPAR_SCUGIC_0_DEVICE_ID);
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT,
                                 (Xil_ExceptionHandler)XScuGic_DeviceInterruptHandler,
                                 (void *)XPAR_SCUGIC_0_DEVICE_ID);
}

static void ecg_net_enable_exception(void)
{
    Xil_ExceptionEnableMask(XIL_EXCEPTION_IRQ);
}

static void ecg_net_print_ip(const char *name, const ip_addr_t *ip)
{
    xil_printf("%s%d.%d.%d.%d\r\n",
               name,
               ip4_addr1(ip),
               ip4_addr2(ip),
               ip4_addr3(ip),
               ip4_addr4(ip));
}

static int ecg_net_udp_send(const void *data, u16 len)
{
    struct pbuf *p;
    err_t err;

    if (!g_ready || g_wave_pcb == 0 || data == 0 || len == 0U) {
        return -1;
    }

    p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (p == 0) {
        return -1;
    }

    memcpy(p->payload, data, len);
    err = udp_send(g_wave_pcb, p);
    pbuf_free(p);

    return (err == ERR_OK) ? 0 : -1;
}

int ecg_net_init(void)
{
    ip_addr_t ipaddr;
    ip_addr_t netmask;
    ip_addr_t gateway;
    unsigned char mac_ethernet_address[] = {
        0x00U, 0x0aU, 0x35U, 0x00U, 0x01U, 0x10U
    };

    IP4_ADDR(&ipaddr,
             ECG_NET_BOARD_IP0,
             ECG_NET_BOARD_IP1,
             ECG_NET_BOARD_IP2,
             ECG_NET_BOARD_IP3);
    IP4_ADDR(&netmask, 255U, 255U, 255U, 0U);
    IP4_ADDR(&gateway,
             ECG_NET_PEER_IP0,
             ECG_NET_PEER_IP1,
             ECG_NET_PEER_IP2,
             ECG_NET_PEER_IP3);
    IP4_ADDR(&g_peer_ip,
             ECG_NET_PEER_IP0,
             ECG_NET_PEER_IP1,
             ECG_NET_PEER_IP2,
             ECG_NET_PEER_IP3);

    ecg_net_setup_exception();
    lwip_init();

    if (xemac_add(&server_netif,
                  &ipaddr,
                  &netmask,
                  &gateway,
                  mac_ethernet_address,
                  XPAR_XEMACPS_0_BASEADDR) == 0) {
        xil_printf("[NET] xemac_add failed\r\n");
        return -1;
    }

    netif_set_default(&server_netif);
    netif_set_up(&server_netif);
    ecg_net_enable_exception();

    g_wave_pcb = udp_new();
    if (g_wave_pcb == 0) {
        xil_printf("[NET] udp_new failed\r\n");
        return -1;
    }

    if (udp_connect(g_wave_pcb, &g_peer_ip, ECG_NET_UDP_PORT) != ERR_OK) {
        xil_printf("[NET] udp_connect failed\r\n");
        return -1;
    }

    g_ready = 1U;

    xil_printf("[NET] UDP ECG stream ready\r\n");
    ecg_net_print_ip("[NET] Board IP: ", &ipaddr);
    ecg_net_print_ip("[NET] Peer  IP: ", &g_peer_ip);
    xil_printf("[NET] UDP port: %lu\r\n", (unsigned long)ECG_NET_UDP_PORT);

    return 0;
}

void ecg_net_poll(u32 now_ms)
{
    static u32 last_arp_ms;
    if (g_ready) {
        if ((u32)(now_ms - last_arp_ms) >= ARP_TMR_INTERVAL) {
            last_arp_ms = now_ms;
            etharp_tmr();
        }
        xemacif_input(&server_netif);
    }
}

u8 ecg_net_is_ready(void)
{
    return g_ready;
}

void ecg_net_send_wave_sample(u64 sample_index,
                              u32 timestamp_ms,
                              s16 filtered,
                              u8 class_id,
                              u8 flags)
{
    EcgNetWavePacket pkt;

    pkt.magic = ECG_NET_MAGIC;
    pkt.version = ECG_NET_VERSION;
    pkt.type = ECG_NET_TYPE_WAVE;
    pkt.seq = g_seq++;
    pkt.sample_index = sample_index;
    pkt.timestamp_ms = timestamp_ms;
    pkt.filtered = filtered;
    pkt.class_id = class_id;
    pkt.flags = flags;

    (void)ecg_net_udp_send(&pkt, (u16)sizeof(pkt));
}

void ecg_net_send_cnn_event(u64 sample_index,
                            u32 timestamp_ms,
                            u32 rr_ms,
                            u32 hr_bpm,
                            u8 class_id,
                            u8 confidence)
{
    EcgNetEventPacket pkt;

    if (class_id == ECG_CLASS_NORMAL) {
        return;
    }

    g_abnormal_count++;
    g_event_id++;

    pkt.magic = ECG_NET_MAGIC;
    pkt.version = ECG_NET_VERSION;
    pkt.type = ECG_NET_TYPE_EVENT;
    pkt.seq = g_seq++;
    pkt.event_id = g_event_id;
    pkt.sample_index = sample_index;
    pkt.timestamp_ms = timestamp_ms;
    pkt.rr_ms = rr_ms;
    pkt.hr_bpm = hr_bpm;
    pkt.abnormal_count = g_abnormal_count;
    pkt.class_id = class_id;
    pkt.confidence = confidence;
    pkt.pre_seconds = 3U;
    pkt.post_seconds = 3U;

    (void)ecg_net_udp_send(&pkt, (u16)sizeof(pkt));
}

u32 ecg_net_abnormal_count(void)
{
    return g_abnormal_count;
}
