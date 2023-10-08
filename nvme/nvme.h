#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../pci/pci.h"

struct NVMERegisters {
	uint64_t Capabilitires;
	uint32_t Version;
	/* TODO */
}__attribute__((packed));

class NVMEDriver {
	public:
		NVMEDriver(PCIDeviceHeader *pciBaseAddress);
		~NVMEDriver();
	private:
		PCIHeader0 *PCIBaseAddress;
		uintptr_t IOBASE;

		volatile NVMERegisters *Registers;
};
