#pragma once

////////////////////////////////////////////////////////////////
// MumanchuDebug.h
// Macro definitions for debugging and logging
// Copyright (C) 2026, muman.ch
// All rights reversed
// https://github.com/mumanchu/

// Comment this out for Release mode
#define DEBUG

#ifdef DEBUG
extern void LogError(const char* msg, const char* filePath, uint line);
#define LOGERROR(msg) { LogError(msg, __FILE__, __LINE__); }
#define ASSERT(b) { if(!(b)) { LOGERROR("ASSERT failed, " #b); return false; } }
#else
#define LOGERROR(msg)
#define ASSERT(b)
#endif


// TODO Paste this code at the top of the main '.ino' file, 
// before any mumanchu library #include files
/*
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
// In DEBUG mode, detect and log errors
#include "MumanchuDebug.h"

#ifdef DEBUG
// if using a debugger, disable all GCC compiler optimisations
//#pragma GCC optimize ("-O0")

// Shared error logging function
void LogError(const char* msg, const char* filePath, uint line)
{
	char buf[256];
	const char* fname = strrchr(filePath, '\\');
	fname = fname ? fname + 1 : filePath;
	sprintf(buf, "ERROR: %s : %s(%u)", msg, fname, line);
	Serial.println(buf);
	Serial.flush();
}
#endif
//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
*/


