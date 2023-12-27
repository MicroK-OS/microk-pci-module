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

extern "C" size_t OnInit() {
	EnumeratePCI(-1);

	return 0;
}

extern "C" size_t OnExit() {

	return 0;
}
