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
	
	MKMI_Printf("XHCI controller base addr: 0x%x\r\n", IOBASE);
	VMMap(IOBASE, IOBASE, 4096, 0);

	CapabilityRegisters = (volatile XHCICapabilityRegisters*)IOBASE;
	OperationalRegisters = (volatile XHCIOperationalRegisters*)(IOBASE + CapabilityRegisters->CapabilityRegisterLength);
	PortRegisters = (volatile XHCIPortRegisters*)(IOBASE + 0x400);
	RuntimeRegisters = (volatile XHCIRuntimeRegisters*)(IOBASE + CapabilityRegisters->RuntimeRegistersSpaceOffset);
	VMMap(RuntimeRegisters, RuntimeRegisters, 4096, 0);

	/* Reset the controller */
	OperationalRegisters->USBCommand = 0x2;
	while(OperationalRegisters->USBStatus & (1 << 12))
		/* Reset in progress */;

	uint8_t maxDeviceSlots = (CapabilityRegisters->StructuralParameters1 & 0x000000FF);
	uint16_t maxInterrupters = (CapabilityRegisters->StructuralParameters1 & 0x00FFFF00) >> 8;
	uint8_t maxPorts = (CapabilityRegisters->StructuralParameters1 & 0xFF000000) >> 24;

	uint8_t ist = (CapabilityRegisters->StructuralParameters2 & 0x0000000F);
	uint8_t erstMax = (CapabilityRegisters->StructuralParameters2 & 0x000000F0) >> 4;
	uint8_t maxScratchpadBuffersHigh = (CapabilityRegisters->StructuralParameters2 & 0x00F80000) >> 22;
	uint8_t spr = (CapabilityRegisters->StructuralParameters2 & 0x01000000) >> 27;
	uint8_t maxScratchpadBuffersLow = (CapabilityRegisters->StructuralParameters2 & 0xF8000000) >> 29;
	uint16_t maxScratchpadBuffers = maxScratchpadBuffersLow + (maxScratchpadBuffersHigh << 10);

	uint8_t u1DeviceExitLatency = (CapabilityRegisters->StructuralParameters3 & 0x000000FF);
	uint8_t u2DeviceExitLatency = (CapabilityRegisters->StructuralParameters3 & 0x0000FF00) >> 8;

	MKMI_Printf("Capability registers:          0x%x\r\n"
		    " Length:                        0x%x\r\n"
		    " Interface version:             0x%x\r\n"
		    " Structural parameters 1:       0x%x\r\n"
		    "  Device slots:                  0x%x\r\n"
		    "  Interrupters:                  0x%x\r\n"
		    "  Ports:                         0x%x\r\n"
		    " Structural parameters 2:       0x%x\r\n"
		    "  IST:                           0x%x\r\n"
		    "  ERST:                          0x%x\r\n"
		    "  Scratchpad buffers:            0x%x\r\n"
		    "   High:                          0x%x\r\n"
		    "   Low:                           0x%x\r\n"
		    "  Scratchpad restore:            0x%x\r\n"
		    " Structural parameters 3:       0x%x\r\n"
		    "  U1 device exit latency:        0x%x\r\n"
		    "  U2 device exit latency:        0x%x\r\n"
		    " Capability parameters 3:       0x%x\r\n",
		    CapabilityRegisters,
		    CapabilityRegisters->CapabilityRegisterLength,
		    CapabilityRegisters->InterfaceVersionNumber,
		    CapabilityRegisters->StructuralParameters1,
		    maxDeviceSlots,
		    maxInterrupters,
		    maxPorts,
		    CapabilityRegisters->StructuralParameters2,
		    ist,
		    erstMax,
		    maxScratchpadBuffers,
		    maxScratchpadBuffersHigh,
		    maxScratchpadBuffersLow,
		    spr,
		    CapabilityRegisters->StructuralParameters3,
		    u1DeviceExitLatency,
		    u2DeviceExitLatency,
		    CapabilityRegisters->CapabilityParameters1);

	MKMI_Printf("Operational registers:         0x%x\r\n",
		    OperationalRegisters);

	MKMI_Printf("Port registers:                0x%x\r\n",
		    PortRegisters);

	for (uint8_t port = 0; port < maxPorts; ++port) {
		volatile XHCIPortRegisters *portRegisters = (volatile XHCIPortRegisters*)((uintptr_t)PortRegisters + port * sizeof(XHCIPortRegisters));
		MKMI_Printf(" Port %d:                       0x%x\r\n"
			    "  Port status control:           0x%x\r\n",
			    port,
			    (uintptr_t)portRegisters,
			    portRegisters->PortStatusControl);
	}

	MKMI_Printf("Runtime registers:             0x%x\r\n",
		    RuntimeRegisters);

	MKMI_Printf("XHCI driver initialized.\r\n");
}


XHCIDriver::~XHCIDriver() {

}
