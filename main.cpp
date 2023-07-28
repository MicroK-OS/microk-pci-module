#include <stdint.h>
#include <stddef.h>

#include <cdefs.h>
#include <mkmi.h>

extern "C" uint32_t VendorID = /* Vendor name here */;
extern "C" uint32_t ProductID = /* Product name here */;

extern "C" size_t OnInit() {
	return 0;
}

extern "C" size_t OnExit() {
	return 0;
}
