#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include <cdefs.h>
#include <mkmi.h>
#include <mkmi_log.h>
#include <mkmi_exit.h>
#include <mkmi_memory.h>
#include <mkmi_message.h>
#include <mkmi_syscall.h>

#include "pci/pci.h"

extern "C" uint32_t VendorID = 0xCAFEBABE;
extern "C" uint32_t ProductID = 0xB830C0DE;

void MessageHandler() {
	MKMI_Message *msg = 0x700000000000;
	uint64_t *data = (uint64_t*)((uintptr_t)msg + 128);

	MKMI_Printf("Message at        0x%x\r\n"
		    " Sender Vendor ID:  %x\r\n"
		    " Sender Product ID: %x\r\n"
		    " Message Size:      %d\r\n"
		    " Data:            0x%x\r\n",
		    msg,
		    msg->SenderVendorID,
		    msg->SenderProductID,
		    msg->MessageSize,
		    *data);

	/* Here we check whether it exists 
	 * If it isn't there, just skip this step
	 */
	if (*data) {
		MKMI_Printf("MCFG at 0x%x\r\n", *data);

		Syscall(SYSCALL_MODULE_SECTION_REGISTER, "PCI", VendorID, ProductID, 0, 0 ,0);
		Syscall(SYSCALL_MODULE_SECTION_REGISTER, "PCI", VendorID, ProductID, 0, 0 ,0);

		EnumeratePCI(*data);
	} else {
		MKMI_Printf("No MCFG found.\r\n");
	}
	
	VMFree(data, msg->MessageSize);
	_return(0);
}

extern "C" size_t OnInit() {
	Syscall(SYSCALL_MODULE_MESSAGE_HANDLER, MessageHandler, 0, 0, 0, 0, 0);

	uint32_t pid = 0, vid = 0;
/*	Syscall(SYSCALL_MODULE_SECTION_GET, "VFS", &vid, &pid, 0, 0 ,0);
	MKMI_Printf("VFS -> VID: %x PID: %x\r\n", vid, pid);

	uintptr_t bufAddr = 0xD000000000;
	size_t bufSize = 4096 * 2;
	uint32_t bufID;
	Syscall(SYSCALL_MODULE_BUFFER_CREATE, bufSize, 0x02, &bufID, 0, 0, 0);
	Syscall(SYSCALL_MODULE_BUFFER_MAP, bufAddr, bufID, 0, 0, 0, 0);
	*(uint32_t*)bufAddr = 69;

	uint8_t msg[256];
	uint32_t *data = (uint32_t*)((uintptr_t)msg + 128);
	*data = bufID;

	Syscall(SYSCALL_MODULE_MESSAGE_SEND, vid, pid, msg, 256, 0 ,0);

	Free(msg);
*/
	Syscall(SYSCALL_MODULE_SECTION_GET, "ACPI", &vid, &pid, 0, 0 ,0);
	MKMI_Printf("ACPI -> VID: %x PID: %x\r\n", vid, pid);

	uint8_t msg[256];
	uint64_t *data = (uint64_t*)((uintptr_t)msg + 128);
	*data = 0x69696969;

	Syscall(SYSCALL_MODULE_MESSAGE_SEND, vid, pid, msg, 256, 0 ,0);

	Free(msg);

	return 0;
}

extern "C" size_t OnExit() {

	return 0;
}
