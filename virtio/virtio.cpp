#include "virtio.h"

#include <mkmi.h>
		
static size_t QueueMappingArea = 0x1000000000;

const char *TypeStrings[] = {
	"Unknown",
	"Network card",
	"Block device",
	"Console",
	"Entropy source",
	"Memory ballooning",
	"I/O memory",
	"RPMSG",
	"SCSI host",
	"9P transport",
	"MAC802.11 WLAN"
};

static inline const char *GetTypeString(VirtIODeviceType type) {
	return TypeStrings[type];	
}

static inline uint32_t DisableFeature(uint32_t features, uint32_t feature) {
	features &= ~(1 << feature);
	return features;
}

VirtIODriver::VirtIODriver(PCIDeviceHeader *pciBaseAddress) {
	PCIBaseAddress = (PCIHeader0*)pciBaseAddress;

	Type = GetDeviceType();

	MKMI_Printf("VirtIO device type: %s\r\n", GetTypeString(Type));

	/* Aligning IOBASE to avoid trouble */
	IOBASE = PCIBaseAddress->BAR0 - (PCIBaseAddress->BAR0 % 0x10);
	uint8_t status = VIRTIO_DEVICE_STATUS_ACKNOWLEDGE;

	OutPort(IOBASE + VIRTIO_REGISTER_DEVICE_STATUS, status, 8);

	if(Type != VirtIODeviceType::NETWORK_CARD && Type != VirtIODeviceType::BLOCK_DEVICE) {
		status |= VIRTIO_DEVICE_STATUS_FAILED;
		OutPort(IOBASE + VIRTIO_REGISTER_DEVICE_STATUS, status, 8);

		return;
	}

	status |= VIRTIO_DEVICE_STATUS_DRIVER;
	OutPort(IOBASE + VIRTIO_REGISTER_DEVICE_STATUS, status, 8);

	size_t result = 0;

	switch(Type) {
		case VirtIODeviceType::NETWORK_CARD:
			result = NetworkCardInitialize(status);
			break;
		case VirtIODeviceType::BLOCK_DEVICE: 
			result = BlockDeviceInitialize(status);
			break;
	}
	
	status |= result;
	OutPort(IOBASE + VIRTIO_REGISTER_DEVICE_STATUS, status, 8);

	MKMI_Printf("Driver initialized.\r\n");
}

size_t VirtIODriver::NetworkCardInitialize(uint8_t status) {
	uint32_t features = GetFeatures();

	SetFeatures(features);

	status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;
	OutPort(IOBASE + VIRTIO_REGISTER_DEVICE_STATUS, status, 8);

	status = InPort(IOBASE + VIRTIO_REGISTER_DEVICE_STATUS, 8);
	if ((status & VIRTIO_DEVICE_STATUS_FEATURES_OK) == 0) {
		MKMI_Printf("Feature set not accepted\r\n");
        	return VIRTIO_DEVICE_STATUS_FAILED;
	}

	/* Setup queue */
	for(int i = 0; i < 16; i++) InitQueue(i);

	status |= VIRTIO_DEVICE_STATUS_DRIVER_OK;
	return status;
}

size_t VirtIODriver::BlockDeviceInitialize(uint8_t status) {
	uint32_t features = GetFeatures();

	features = DisableFeature(features, VIRTIO_BLOCK_FLAGS_RO);
	features = DisableFeature(features, VIRTIO_BLOCK_FLAGS_BLOCK_SIZE);
	features = DisableFeature(features, VIRTIO_BLOCK_FLAGS_TOPOLOGY);
	
	SetFeatures(features);

	status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;
	OutPort(IOBASE + VIRTIO_REGISTER_DEVICE_STATUS, status, 8);

	status = InPort(IOBASE + VIRTIO_REGISTER_DEVICE_STATUS, 8);
	if ((status & VIRTIO_DEVICE_STATUS_FEATURES_OK) == 0) {
		MKMI_Printf("Feature set not accepted\r\n");
        	return VIRTIO_DEVICE_STATUS_FAILED;
	}

	/* Setup queue */
	for(int i = 0; i < 16; i++) InitQueue(i);

	uint64_t sectors = 0;
	sectors |= InPort(IOBASE + 0x14, 32);
	sectors |= InPort(IOBASE + 0x18, 32) << 32;

	MKMI_Printf("Block device with %d sectors. Size: %dMB\r\n", sectors, sectors / 2048);

	status |= VIRTIO_DEVICE_STATUS_DRIVER_OK;
	return status;
}

VirtIODriver::~VirtIODriver() {

}

bool VirtIODriver::InitQueue(size_t index) {
	uint16_t queueSize; 

	volatile VirtIOQueue *virtualQueue = &Queues[index];
	Memset(virtualQueue, 0, sizeof(VirtIOQueue));

	/* Get the queue size */ 
	OutPort(IOBASE + VIRTIO_REGISTER_QUEUE_SELECT, index, 16);
	queueSize = InPort(IOBASE + VIRTIO_REGISTER_QUEUE_SIZE, 16);
	virtualQueue->QueueSize = queueSize;
	if (queueSize == 0) return false;
	
	// create virtqueue memory
	uint32_t sizeofBuffers = (sizeof(VirtIOQueueBuffer) * queueSize);
	uint32_t sizeofQueueAvailable = (2 * sizeof(uint16_t)) + (queueSize * sizeof(uint16_t)); 
	uint32_t sizeofQueueUsed = (2 * sizeof(uint16_t)) + (queueSize * sizeof(VirtIOQueueUsedItem));
	size_t totalSize = sizeofBuffers + sizeofQueueUsed + sizeofQueueAvailable;
	uint8_t *buffer = PMAlloc(totalSize);

	VMMap(buffer, QueueMappingArea, totalSize, 0);
	Memset(QueueMappingArea, 0, totalSize);

	uint32_t bufPage = (uintptr_t)buffer >> 12;

	virtualQueue->BaseAddress = (uint64_t)buffer;
	virtualQueue->Available = (VirtIOQueueAvailable*)&buffer[sizeofBuffers];
	virtualQueue->Used = (VirtIOQueueUsed*)&buffer[((sizeofBuffers + sizeofQueueAvailable+0xFFF)&~0xFFF)];
	virtualQueue->NextBuffer = 0;
	virtualQueue->Lock = 0;

	OutPort(IOBASE + VIRTIO_REGISTER_QUEUE_ADDRESS, bufPage, 32);

	/* Essentially, we're pointing to virtualQueue->Available */
	volatile VirtIOQueueAvailable *available = (VirtIOQueueAvailable*)(QueueMappingArea + &buffer[sizeofBuffers] - buffer);
	available->Flags = 0;

	QueueMappingArea += totalSize + (4096 - totalSize % 4096);

	return true;

}

VirtIODeviceType VirtIODriver::GetDeviceType() {
	if(PCIBaseAddress->SubsystemID > 10)
		return VirtIODeviceType::UNKNOWN;

	return static_cast<VirtIODeviceType>(PCIBaseAddress->SubsystemID);
}

uint32_t VirtIODriver::GetFeatures() {
	size_t features = InPort(IOBASE + VIRTIO_REGISTER_DEVICE_FEATURES, 32);

	return (uint32_t)features;
}
		
void VirtIODriver::SetFeatures(uint32_t features) {
	OutPort(IOBASE + VIRTIO_REGISTER_GUEST_FEATURES, features, 32);
}


