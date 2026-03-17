/* ============================================================
 * SwanOS — Network Module
 * PCI bus scanning for NIC detection + simulated connection state.
 * Provides status info for the Network Settings UI.
 * ============================================================ */

#include "network.h"
#include "ports.h"
#include "string.h"
#include "serial.h"

/* ── PCI Configuration Space I/O ──────────────────────────── */
#define PCI_CONFIG_ADDR  0xCF8
#define PCI_CONFIG_DATA  0xCFC

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) |
                       (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(PCI_CONFIG_ADDR, address);
    return inl(PCI_CONFIG_DATA);
}

/* ── Known NIC identifiers ────────────────────────────────── */
typedef struct {
    uint16_t vendor;
    uint16_t device;
    const char *name;
    int speed;
} known_nic_t;

static const known_nic_t known_nics[] = {
    { 0x8086, 0x100E, "Intel e1000",         1000 },  /* QEMU default */
    { 0x8086, 0x100F, "Intel e1000",         1000 },
    { 0x8086, 0x10D3, "Intel 82574L",        1000 },
    { 0x8086, 0x153A, "Intel I217-LM",       1000 },
    { 0x8086, 0x15A3, "Intel I219-LM",       1000 },
    { 0x1AF4, 0x1000, "VirtIO Net",          1000 },  /* VirtIO legacy */
    { 0x1AF4, 0x1041, "VirtIO Net 1.0",      1000 },  /* VirtIO modern */
    { 0x10EC, 0x8139, "Realtek RTL8139",      100 },  /* Common in VMs */
    { 0x10EC, 0x8168, "Realtek RTL8168",     1000 },
    { 0x1022, 0x2000, "AMD PCnet-PCI",        100 },  /* VirtualBox */
    { 0x14E4, 0x1677, "Broadcom NetXtreme",  1000 },
    { 0, 0, NULL, 0 }
};

/* ── Module state ─────────────────────────────────────────── */
static net_status_t net_state;

void net_init(void) {
    memset(&net_state, 0, sizeof(net_state));
    strcpy(net_state.net_type, "Ethernet");
    strcpy(net_state.ip_addr, "No IP");
    strcpy(net_state.mac_addr, "00:00:00:00:00:00");
    strcpy(net_state.gateway, "0.0.0.0");
    strcpy(net_state.dns, "0.0.0.0");
    strcpy(net_state.nic_name, "No adapter");
    net_state.detected = 0;
    net_state.connected = 0;
    net_state.link_speed = 0;

    /* Scan PCI bus 0 for known NICs */
    serial_write("net_init: scanning PCI bus...\n");
    for (int bus = 0; bus < 2; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                uint32_t reg0 = pci_config_read(bus, slot, func, 0);
                uint16_t vendor = reg0 & 0xFFFF;
                uint16_t device = (reg0 >> 16) & 0xFFFF;

                if (vendor == 0xFFFF) continue; /* No device */

                /* Check against known NICs */
                for (int k = 0; known_nics[k].name != NULL; k++) {
                    if (vendor == known_nics[k].vendor && device == known_nics[k].device) {
                        net_state.detected = 1;
                        net_state.vendor_id = vendor;
                        net_state.device_id = device;
                        net_state.link_speed = known_nics[k].speed;
                        strcpy(net_state.nic_name, known_nics[k].name);

                        /* Set simulated network parameters */
                        net_state.connected = 1;
                        strcpy(net_state.ip_addr, "10.0.2.15");
                        strcpy(net_state.mac_addr, "52:54:00:12:34:56");
                        strcpy(net_state.gateway, "10.0.2.2");
                        strcpy(net_state.dns, "10.0.2.3");

                        serial_write("net_init: found NIC: ");
                        serial_write(known_nics[k].name);
                        serial_write("\n");
                        return; /* Found a NIC, done scanning */
                    }
                }
            }
        }
    }
    serial_write("net_init: no NIC found\n");
}

net_status_t *net_get_status(void) {
    return &net_state;
}

void net_toggle_connection(void) {
    if (!net_state.detected) return; /* Can't connect without NIC */

    net_state.connected = !net_state.connected;
    if (net_state.connected) {
        strcpy(net_state.ip_addr, "10.0.2.15");
        strcpy(net_state.gateway, "10.0.2.2");
        strcpy(net_state.dns, "10.0.2.3");
    } else {
        strcpy(net_state.ip_addr, "No IP");
        strcpy(net_state.gateway, "0.0.0.0");
        strcpy(net_state.dns, "0.0.0.0");
    }
}

const char *net_status_str(void) {
    if (!net_state.detected) return "No adapter";
    return net_state.connected ? "Connected" : "Disconnected";
}
