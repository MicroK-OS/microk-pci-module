#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../pci/pci.h"

class IntelHDADriver {
	public:
		IntelHDADriver(PCIDeviceHeader *pciBaseAddress);
		~IntelHDADriver();
	private:
		PCIHeader0 *PCIBaseAddress;
		uintptr_t IOBASE;
};
