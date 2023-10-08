#include "intel-hda.h"

#include <mkmi.h>

IntelHDADriver::IntelHDADriver(PCIDeviceHeader *pciBaseAddress) {
	PCIBaseAddress = (PCIHeader0*)pciBaseAddress;
	
	MKMI_Printf("Intel HDA driver.\r\n");
}


IntelHDADriver::~IntelHDADriver() {

}
