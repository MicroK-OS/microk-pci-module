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
	VMMap(newBase, newBase, 4096, 0);

	HBAPortPtr->CommandListBase = (uint32_t)(uint64_t)newBase;
	HBAPortPtr->CommandListBaseUpper = (uint32_t)((uint64_t)newBase >> 32);
	
	Memset(newBase, 0, 1024);

	void *fisBase = PMAlloc(4096);
	VMMap(fisBase, fisBase, 4096, 0);

	HBAPortPtr->FISBaseAddress = (uint32_t)(uint64_t)fisBase;
	HBAPortPtr->FISBaseAddressUpper = (uint32_t)((uint64_t)fisBase >> 32);

	Memset(fisBase, 0, 256);

	HBACommandHeader *commandHeader = (HBACommandHeader*)newBase;

	for (int i = 0; i < 32; i++){
		commandHeader[i].PRDTLength = 8;

		void *commandTableAddress = PMAlloc(4096);
		VMMap(commandTableAddress, commandTableAddress, 4096, 0);

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
	commandFIS->LBA5 = (uint8_t)(sectorHigh >> 16);

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

struct MBRPartition {
	uint8_t Attributes;
	uint32_t CHSAddressPartitionStart : 24;
	uint8_t Type;
	uint32_t CHSAddressPartitionLast : 24;
	uint32_t LBAPartitionStart;
	uint32_t NumberSectors;
}__attribute__((packed));

struct MBR {
	uint8_t Bootstrap[440];
	uint8_t UniqueDiskID[4];
	uint16_t Reserved0;

	MBRPartition FirstPartition;
	MBRPartition SecondPartition;
	MBRPartition ThirdPartition;
	MBRPartition FourthPartition;

	uint8_t Signature[2];
}__attribute__((packed));

struct GPT {
	uint8_t Signature[8];
	uint32_t Revision;
	uint32_t HeaderSize;
	uint32_t CRC32Checksum;
	uint32_t Reserved0;
	uint64_t ThisLBA;
	uint64_t AlternateLBA;
	uint64_t FirstUsable;
	uint64_t LastUsable;
	uint8_t GUID[16];
	uint64_t PartitionEntryStartLBA;
	uint32_t PartitionEntries;
	uint32_t SizePartitonEntry;
	uint32_t CRC32ChecksumPartitionEntries;
}__attribute__((packed));

struct GPTPartitionEntry {
	uint8_t Type[16];
	uint8_t GUID[16];

	uint64_t StartingLBA;
	uint64_t EndingLBA;

	uint64_t Attributes;

	uint8_t PartitionName[1];
}__attribute__((packed));

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
		VMMap(port->Buffer, port->Buffer, 4096, 0);

		MKMI_Printf("Reading from port %d...\r\n", i);

		Memset((void*)port->Buffer, 0, 0x1000);

		if(!port->Read(0, 4, port->Buffer)) break;

		MBR *mbrPartitionTable = (MBR*)port->Buffer;

		if(mbrPartitionTable->Signature[0] != 0x55 || mbrPartitionTable->Signature[1] != 0xAA) {
			MKMI_Printf("Invalid partition table.\r\n");
		} else {
			if (mbrPartitionTable->FirstPartition.Type == 0xEE) {
				GPT *guidPartitionTable = (GPT*)((uintptr_t)port->Buffer + 512);

				MKMI_Printf("We've got a GPT partition table (%s) with %d partitions.\r\n", guidPartitionTable->Signature, guidPartitionTable->PartitionEntries);

				for (uintptr_t position = 0; position < guidPartitionTable->PartitionEntries * guidPartitionTable->SizePartitonEntry; position += guidPartitionTable->SizePartitonEntry) {
					GPTPartitionEntry *entry = (GPTPartitionEntry*)((uintptr_t)port->Buffer + guidPartitionTable->PartitionEntryStartLBA * 512 + position);
					if(entry->StartingLBA == 0 && entry->EndingLBA == 0) break;
					MKMI_Printf("Entry from %d to %d\r\n", entry->StartingLBA, entry->EndingLBA);
				}
			} else {
				MKMI_Printf("We've got a MBR partition table\r\n");
			}
		}

		VMFree(port->Buffer, 0x1000);
	}
}

AHCIDriver::~AHCIDriver(){

}
