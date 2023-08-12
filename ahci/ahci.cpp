#include "ahci.h"

#include <mkmi.h>

#define HBA_PORT_DEV_PRESENT 0x3
#define HBA_PORT_IPM_ACTIVE 0x1
#define SATA_SIG_ATAPI 0xEB140101
#define SATA_SIG_ATA 0x00000101
#define SATA_SIG_SEMB 0xC33C0101
#define SATA_SIG_PM 0x96690101

#define HBA_PxCMD_CR 0x8000
#define HBA_PxCMD_FRE 0x0010
#define HBA_PxCMD_ST 0x0001
#define HBA_PxCMD_FR 0x4000

PortType CheckPortType(HBAPort *port){
	uint32_t sataStatus = port->SataStatus;

	uint8_t interfacePowerManagement = (sataStatus >> 8) & 0b111;
	uint8_t deviceDetection = sataStatus & 0b111;

	if (deviceDetection != HBA_PORT_DEV_PRESENT) return PortType::None;
	if (interfacePowerManagement != HBA_PORT_IPM_ACTIVE) return PortType::None;

	switch (port->Signature){
		case SATA_SIG_ATAPI:
			return PortType::SATAPI;
		case SATA_SIG_ATA:
			return PortType::SATA;
		case SATA_SIG_PM:
			return PortType::PM;
		case SATA_SIG_SEMB:
			return PortType::SEMB;
		default:
			return PortType::None;
	}
}

void AHCIDriver::ProbePorts(){
	uint32_t portsImplemented = ABAR->PortsImplemented;

	for (int i = 0; i < 32; i++){
		if (portsImplemented & (1 << i)){
			PortType portType = CheckPortType(&ABAR->Ports[i]);

			if (portType == PortType::SATA || portType == PortType::SATAPI) {
				Ports[PortCount] = new Port();
				Ports[PortCount]->AHCIPortType = portType;
				Ports[PortCount]->HBAPortPtr = &ABAR->Ports[i];
				Ports[PortCount]->PortNumber = PortCount;
				PortCount++;
			}
		}
	}
}

void Port::Configure(){
	StopCMD();

	void *newBase = PMAlloc(4096);

	HBAPortPtr->CommandListBase = (uint32_t)(uint64_t)newBase;
	HBAPortPtr->CommandListBaseUpper = (uint32_t)((uint64_t)newBase >> 32);
	
	Memset(newBase, 0, 1024);

	void *fisBase = PMAlloc(4096);

	HBAPortPtr->FISBaseAddress = (uint32_t)(uint64_t)fisBase;
	HBAPortPtr->FISBaseAddressUpper = (uint32_t)((uint64_t)fisBase >> 32);

	Memset(fisBase, 0, 256);

	HBACommandHeader *commandHeader = (HBACommandHeader*)newBase;

	for (int i = 0; i < 32; i++){
		commandHeader[i].PRDTLength = 8;

		void *commandTableAddress = PMAlloc(4096);

		uint64_t address = (uint64_t)commandTableAddress + (i << 8);

		commandHeader[i].CommandTableBaseAddress = (uint32_t)(uint64_t)address;
		commandHeader[i].CommandTableBaseAddressUpper = (uint32_t)((uint64_t)address >> 32);

		Memset(commandTableAddress, 0, 256);
	}

	StartCMD();
}

void Port::StopCMD(){
	HBAPortPtr->CommandStatus &= ~HBA_PxCMD_ST;
	HBAPortPtr->CommandStatus &= ~HBA_PxCMD_FRE;

	while(true){
		if (HBAPortPtr->CommandStatus & HBA_PxCMD_FR) continue;
		if (HBAPortPtr->CommandStatus & HBA_PxCMD_CR) continue;

		break;
	}

}

void Port::StartCMD(){
	while (HBAPortPtr->CommandStatus & HBA_PxCMD_CR);

	HBAPortPtr->CommandStatus |= HBA_PxCMD_FRE;
	HBAPortPtr->CommandStatus |= HBA_PxCMD_ST;
}

bool Port::Read(uint64_t sector, uint32_t sectorCount, volatile void *buffer){
	uint32_t sectorLow = (uint32_t)sector;
	uint32_t sectorHigh = (uint32_t)(sector >> 32);

	HBAPortPtr->InterruptStatus = (uint32_t)-1; /* Clear pending interrupt bits */

	HBACommandHeader *commandHeader = (HBACommandHeader*)((uint64_t)HBAPortPtr->CommandListBase + ((uint64_t)HBAPortPtr->CommandListBaseUpper << 32));
	commandHeader->CommandFISLength = sizeof(FIS_REG_H2D)/ sizeof(uint32_t); /* Command FIS size */
	commandHeader->Write = 0; /* This is a read */
	commandHeader->PRDTLength = 1;

	HBACommandTable *commandTable = (HBACommandTable*)((uint64_t)commandHeader->CommandTableBaseAddress + ((uint64_t)commandHeader->CommandTableBaseAddressUpper << 32));
	Memset(commandTable, 0, sizeof(HBACommandTable) + (commandHeader->PRDTLength-1) * sizeof(HBAPRDTEntry));

	commandTable->PRDTEntry[0].DataBaseAddress = (uint32_t)(uint64_t)buffer;
	commandTable->PRDTEntry[0].DataBaseAddressUpper = (uint32_t)((uint64_t)buffer >> 32);
	commandTable->PRDTEntry[0].ByteCount = (sectorCount << 9) - 1; /* 512 bytes per sector */
	commandTable->PRDTEntry[0].InterruptOnCompletion = 1;

	FIS_REG_H2D *commandFIS = (FIS_REG_H2D*)(&commandTable->CommandFIS);

	commandFIS->FISType = FIS_TYPE_REG_H2D;
	commandFIS->CommandControl = 1; /* Command */
	commandFIS->Command = ATA_CMD_READ_DMA_EX;

	commandFIS->LBA0 = (uint8_t)sectorLow;
	commandFIS->LBA1 = (uint8_t)(sectorLow >> 8);
	commandFIS->LBA2 = (uint8_t)(sectorLow >> 16);
	commandFIS->LBA3 = (uint8_t)sectorHigh;
	commandFIS->LBA4 = (uint8_t)(sectorHigh >> 8);
	commandFIS->LBA4 = (uint8_t)(sectorHigh >> 16);

	commandFIS->DeviceRegister = 1 << 6; /* LBA mode */

	commandFIS->CountLow = sectorCount & 0xFF;
	commandFIS->CountHigh = (sectorCount >> 8) & 0xFF;

	uint64_t spin = 0;

	while ((HBAPortPtr->TaskFileData & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) {
		spin++;
	}

	if (spin == 1000000000) {
		return false;
	}

	HBAPortPtr->CommandIssue = 1;

	while (true){
		if(HBAPortPtr->CommandIssue == 0) break;
		if(HBAPortPtr->InterruptStatus & HBA_PxIS_TFES)
		{
			return false;
		}
	}

	return true;
}

AHCIDriver::AHCIDriver(PCIDeviceHeader* pciBaseAddress){
	PCIBaseAddress = pciBaseAddress;
	MKMI_Printf("AHCI Driver instance initialized\r\n");

	ABAR = (HBAMemory*)(uint64_t)((PCIHeader0*)PCIBaseAddress)->BAR5;

	VMMap(ABAR, ABAR, 4096, 0);

	ProbePorts();

	for (int i = 0; i < PortCount; i++){
		Port *port = Ports[i];

		port->Configure();

		port->Buffer = (uint8_t*)PMAlloc(4096);
		Memset((void*)port->Buffer, 0, 0x1000);

		MKMI_Printf("Reading from port %d...\r\n", i);
		port->Read(0, 4, port->Buffer);
		for (int t = 0; t < 1024; t++){
			MKMI_Printf("%x", port->Buffer[t]);
		}

		MKMI_Printf("\r\n");
	}
}

AHCIDriver::~AHCIDriver(){

}
