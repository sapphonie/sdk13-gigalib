#ifndef CSTDLIB_SDK13_INC_H
#define CSTDLIB_SDK13_INC_H

#ifdef _WIN32
#pragma once
#endif


#ifdef min
	#undef min
#endif

#ifdef max
	#undef max
#endif

#include <memdbgoff.h>


#include <chrono>
#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include <algorithm>
#include <thread>
#include <fstream>
#include <filesystem>

#include <fmt/format.h>

#ifndef min
	#define min(a,b)  (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
	#define max(a,b)  (((a) > (b)) ? (a) : (b))
#endif

#ifdef GNUC
	#undef offsetof
	//#define offsetof( type, var ) __builtin_offsetof( type, var )
	#define offsetof(s,m)	(size_t)&(((s *)0)->m)
#else
	#undef offsetof
	#define offsetof(s,m)	(size_t)&(((s *)0)->m)
#endif


#include <memdbgon.h>


#endif
