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


int MessageHandler(MKMI_Message *msg, uint64_t *data) {
	if (*data) {
		MKMI_Printf("MCFG at 0x%x\r\n", *data);

		Syscall(SYSCALL_MODULE_SECTION_REGISTER, "PCI", VendorID, ProductID, 0, 0 ,0);
		Syscall(SYSCALL_MODULE_SECTION_REGISTER, "PCIe", VendorID, ProductID, 0, 0 ,0);

		EnumeratePCI(*data);

		return 0;
	} else {
		MKMI_Printf("No MCFG found.\r\n");

		return 1;
	}

	return 1;
}

extern "C" size_t OnInit() {
	SetMessageHandlerCallback(MessageHandler);
	Syscall(SYSCALL_MODULE_MESSAGE_HANDLER, MKMI_MessageHandler, 0, 0, 0, 0, 0);

	uint32_t pid = 0, vid = 0;
	Syscall(SYSCALL_MODULE_SECTION_GET, "ACPI", &vid, &pid, 0, 0 ,0);
	MKMI_Printf("ACPI -> VID: %x PID: %x\r\n", vid, pid);

	uint8_t msg[256];
	uint64_t *data = (uint64_t*)((uintptr_t)msg + 128);
	*data = 0x69696969;

	SendDirectMessage(vid, pid, msg, 256);

	Free(msg);

	return 0;
}

extern "C" size_t OnExit() {

	return 0;
}
