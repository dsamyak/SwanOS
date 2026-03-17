#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>

/* ── Network status structure ─────────────────────────────── */
typedef struct {
    int      detected;         /* 1 if NIC found on PCI bus */
    int      connected;        /* 1 if simulated connection active */
    char     nic_name[32];     /* e.g. "Intel e1000", "VirtIO Net" */
    char     ip_addr[20];      /* e.g. "10.0.2.15" */
    char     mac_addr[20];     /* e.g. "52:54:00:12:34:56" */
    char     gateway[20];      /* e.g. "10.0.2.2" */
    char     dns[20];          /* e.g. "10.0.2.3" */
    char     net_type[16];     /* "Ethernet" */
    uint16_t vendor_id;
    uint16_t device_id;
    int      link_speed;       /* Mbps (simulated) */
} net_status_t;

/* ── PCI helpers ──────────────────────────────────────────── */
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

/* ── Network API ──────────────────────────────────────────── */
void           net_init(void);
net_status_t  *net_get_status(void);
void           net_toggle_connection(void);
const char    *net_status_str(void);

#endif
