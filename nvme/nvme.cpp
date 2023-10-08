#include "nvme.h"

#include <mkmi.h>

NVMEDriver::NVMEDriver(PCIDeviceHeader *pciBaseAddress) {
	PCIBaseAddress = (PCIHeader0*)pciBaseAddress;

	if((PCIBaseAddress->BAR0 & 0b1) != 0x0 || ((PCIBaseAddress->BAR0 & 0b110) >> 1) != 0x2) {
		IOBASE = NULL;
		MKMI_Printf("NVME driver BARs are not correct.\r\n");

		return;
	} else {
		IOBASE = ((uint64_t)(PCIBaseAddress->BAR0 & 0xFFFFFFF0) + ((uint64_t)(PCIBaseAddress->BAR1 & 0xFFFFFFFF) << 32));
	}

	MKMI_Printf("NVME addr: 0x%x\r\n", IOBASE);
	VMMap(IOBASE, IOBASE, 4096, 0);

	Registers = (NVMERegisters*)IOBASE;

	MKMI_Printf("NVME version: 0x%x\r\n", Registers->Version);
	
	MKMI_Printf("NVME driver.\r\n");
}


NVMEDriver::~NVMEDriver() {

}
