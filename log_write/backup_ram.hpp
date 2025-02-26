#pragma once

#include <cstdarg>              // va_list



namespace backup_ram {

// Build a string from the given parameters, store it in the backup ram, and return
// it as a C-string.
const char* write(const char* format, va_list vlist);

// Return the stored strings as a single concatenated C-string.
const char* read();

};
