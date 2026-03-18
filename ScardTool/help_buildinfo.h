// include/help_buildinfo.h
#ifndef SCARD_TOOL_HELP_BUILDINFO_H
#define SCARD_TOOL_HELP_BUILDINFO_H

#include "build_info.h"

/// Konsolu UTF-8 moduna geçirir (Windows'ta gerekli)
void setup_unicode_console();

/// --help komutunu işler
void print_help();

/// --buildinfo komutunu işler
void print_buildinfo();

#endif // SCARD_TOOL_HELP_BUILDINFO_H
