#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../pci/pci.h"

struct XHCICapabilityRegisters {
	uint8_t CapabilityRegisterLength;
	uint8_t Reserved0;
	uint16_t InterfaceVersionNumber;
	uint32_t StructuralParameters1;
	uint32_t StructuralParameters2;
	uint32_t StructuralParameters3;
	uint32_t CapabilityParameters1;
	uint32_t DorbellOffset;
	uint32_t RuntimeRegistersSpaceOffset;
	uint32_t CapabilityParamenters2;
}__attribute__((packed));

struct XHCIOperationalRegisters {
	uint32_t USBCommand;
	uint32_t USBStatus;
	uint8_t PageSize[12];
	uint32_t DeviceNotificationControl;
	uint64_t CommandRingControl;
	uint64_t DeviceContextBaseAddressArrayPointer;
	uint64_t Configure;
}__attribute__((packed));

struct XHCIPortRegisters {
	uint32_t PortStatusControl;
	uint32_t PortPowerManagment;
	uint32_t PortLinkInfo;
	uint32_t Reserved0;
}__attribute__((packed));

class XHCIDriver {
	public:
		XHCIDriver(PCIDeviceHeader *pciBaseAddress);
		~XHCIDriver();
	private:
		PCIHeader0 *PCIBaseAddress;
		uintptr_t IOBASE;
		
		volatile XHCICapabilityRegisters *CapabilityRegisters;
		volatile XHCIOperationalRegisters *OperationalRegisters;
		volatile XHCIPortRegisters *PortRegisters;
};
