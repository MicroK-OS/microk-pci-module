#include "virtio.h"

#include <mkmi.h>

VirtIODriver::VirtIODriver(PCIDeviceHeader *pciBaseAddress) {
	PCIBaseAddress = (PCIHeader0*)pciBaseAddress;

	MKMI_Printf("VirtIO device: %s\r\n", GetDeviceType());
}


VirtIODriver::~VirtIODriver() {

}


const char *VirtIODriver::GetDeviceType() {
	switch(PCIBaseAddress->SubsystemID) {
		case 1: return "Network Card";
		case 2: return "Block Device";
		case 3: return "Console";
		case 4: return "Entropy Source";
		case 5: return "Memory Ballooning";
		case 6: return "IO Memory";
		case 7: return "RPMSG";
		case 8: return "SCSI Host";
		case 9: return "9P Transport";
		case 10: return " MAC802.11 WLAN";
		return "Unknown";
	}
}
