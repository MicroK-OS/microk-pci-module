#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../pci/pci.h"

class VirtIODriver{
	public:
		VirtIODriver(PCIDeviceHeader *pciBaseAddress);
		~VirtIODriver();
	private:
		const char *GetDeviceType();

		PCIHeader0 *PCIBaseAddress;
};
