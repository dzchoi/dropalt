#define class _class
// The file sys/include/usb/descriptor.h from Riot uses "class" as a struct member name.
// This is not an issue with .c files (e.g., when compiling the stdio_cdc_acm module),
// but we need to patch it here to avoid a compilation error with C++ files.
#include "usb/usbus.h"
#undef class
