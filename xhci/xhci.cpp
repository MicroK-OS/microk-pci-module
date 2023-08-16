#include "xhci.h"

#include <mkmi.h>

XHCIDriver::XHCIDriver(PCIDeviceHeader *pciBaseAddress) {
	PCIBaseAddress = (PCIHeader0*)pciBaseAddress;

	if((PCIBaseAddress->BAR0 & 0b1) != 0x0 || ((PCIBaseAddress->BAR0 & 0b110) >> 1) != 0x2) {
		IOBASE = NULL;
		MKMI_Printf("NVME driver BARs are not correct.\r\n");

		return;
	} else {
		IOBASE = ((uint64_t)(PCIBaseAddress->BAR0 & 0xFFFFFFF0) + ((uint64_t)(PCIBaseAddress->BAR1 & 0xFFFFFFFF) << 32));
	}
	
	MKMI_Printf("XHCI addr: 0x%x\r\n", IOBASE);
	VMMap(IOBASE, IOBASE, 4096, 0);

	CapabilityRegisters = (XHCICapabilityRegisters*)IOBASE;
	OperationalRegisters = (XHCIOperationalRegisters*)(IOBASE + CapabilityRegisters->CapabilityRegisterLength);
	PortRegisters = (XHCIPortRegisters*)(IOBASE + 0x400);
	
	MKMI_Printf("XHCI driver.\r\n");
}


XHCIDriver::~XHCIDriver() {

}
