#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../pci/pci.h"

enum VirtIODeviceType {
	UNKNOWN = 0,
	NETWORK_CARD = 1,
	BLOCK_DEVICE = 2,
	CONSOLE = 3,
	ENTROPY_SOURCE = 4,
	MEMORY_BALLOONING = 5,
	IO_MEMORY = 6,
	RPMSG = 7,
	SCSI_HOST = 8,
	_9P_TRANSPORT = 9,
	MAC802_11_WLAN = 10,
};

#define VIRTIO_REGISTER_DEVICE_FEATURES 0x00
#define VIRTIO_REGISTER_GUEST_FEATURES  0x04
#define VIRTIO_REGISTER_QUEUE_ADDRESS   0x08
#define VIRTIO_REGISTER_QUEUE_SIZE      0x0C
#define VIRTIO_REGISTER_QUEUE_SELECT    0x0E
#define VIRTIO_REGISTER_QUEUE_NOTIFY    0x10
#define VIRTIO_REGISTER_DEVICE_STATUS   0x12
#define VIRTIO_REGISTER_ISR_STATUS      0x13

#define VIRTIO_DEVICE_STATUS_ACKNOWLEDGE   1
#define VIRTIO_DEVICE_STATUS_DRIVER        2
#define VIRTIO_DEVICE_STATUS_FAILED      128
#define VIRTIO_DEVICE_STATUS_FEATURES_OK   8
#define VIRTIO_DEVICE_STATUS_DRIVER_OK     4
#define VIRTIO_DEVICE_NEEDS_RESET         64

/* Network card */
#define VIRTIO_REGISTER_MAC_1           0x14
#define VIRTIO_REGISTER_MAC_2           0x15
#define VIRTIO_REGISTER_MAC_3           0x16
#define VIRTIO_REGISTER_MAC_4           0x17
#define VIRTIO_REGISTER_MAC_5           0x18
#define VIRTIO_REGISTER_MAC_6           0x19
#define VIRTIO_REGISTER_NET_STATUS          0x1A


/* Block device */

#define VIRTIO_BLOCK_FLAGS_SIZE_MAX        1
#define VIRTIO_BLOCK_FLAGS_SEG_MAX         2
#define VIRTIO_BLOCK_FLAGS_GEOMETRY        4
#define VIRTIO_BLOCK_FLAGS_RO              5
#define VIRTIO_BLOCK_FLAGS_BLOCK_SIZE      6
#define VIRTIO_BLOCK_FLAGS_FLUSH           9
#define VIRTIO_BLOCK_FLAGS_TOPOLOGY       10
#define VIRTIO_BLOCK_FLAGS_CONFIG_WCE     11

#define VIRTIO_REGISTER_TOTAL_SECTOR    0x14
#define VIRTIO_REGISTER_MAX_SEG_SIZE    0x1C
#define VIRTIO_REGISTER_MAX_SEC_COUNT   0x20
#define VIRTIO_REGISTER_CYLINDER_COUNT  0x24
#define VIRTIO_REGISTER_HEAD_COUNT      0x26
#define VIRTIO_REGISTER_SECTOR_COUNT    0x27
#define VIRTIO_REGISTER_BLOCK_LENGTH    0x28

struct VirtIOQueueBuffer {
	uint64_t Address;
	uint32_t Length;
	uint16_t Flags;
	uint16_t Next;
}__attribute__((packed));

struct VirtIOQueueAvailable {
	uint16_t Flags;
	uint16_t Index;
	uint16_t Rings[];
}__attribute__((packed));

struct VirtIOQueueUsedItem {
	uint32_t Index;
	uint32_t Length;
}__attribute__((packed));

struct VirtIOQueueUsed {
	uint16_t Flags;
	uint16_t Index;
	VirtIOQueueUsedItem Rings[];
}__attribute__((packed));

struct VirtIOQueue {
	uint16_t QueueSize;
	union {
		uint64_t BaseAddress;
		VirtIOQueueBuffer *Buffers;
	};

	VirtIOQueueAvailable *Available;
	VirtIOQueueUsed *Used;

	uint16_t LastUsedIndex;
	uint16_t LastAvailableIndex;
	uint8_t *Buffer;
	uint32_t ChunkSize;
	uint16_t NextBuffer;
	uint64_t Lock;
}__attribute__((packed));

class VirtIODriver {
	public:
		VirtIODriver(PCIDeviceHeader *pciBaseAddress);
		~VirtIODriver();
	private:
		VirtIODeviceType GetDeviceType();
		uint32_t GetFeatures();
		void SetFeatures(uint32_t features);
		bool InitQueue(size_t index);

		size_t NetworkCardInitialize(uint8_t status);
		size_t BlockDeviceInitialize(uint8_t status);

		PCIHeader0 *PCIBaseAddress;
		uintptr_t IOBASE;
		VirtIODeviceType Type;

		volatile VirtIOQueue Queues[16];
};
