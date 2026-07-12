#pragma once

#include <stdint.h>

namespace PlayerProfile {
uint16_t defaultInitials();
uint16_t loadInitials();
void saveInitials(char first, char second);
void unpackInitials(uint16_t packed, char out[3]);
void unpackDottedInitials(uint16_t packed, char out[4]);
char normalizeInitial(char value);
}
