#ifndef CSTDLIB_SDK13_INC_H
#define CSTDLIB_SDK13_INC_H

#ifdef _WIN32
#pragma once
#endif


#ifdef MEM_OVERRIDE_ON
#define DO_MEM_REOVERRIDE 1
#endif


#ifdef max
#define DO_MINMAX_REOVERRIDE 1
#endif

#ifdef min
	#undef min
#endif

#ifdef max
	#undef max
#endif

#include <memdbgoff.h>

#include <type_traits>
#include <chrono>
#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include <algorithm>
#include <thread>
#include <fstream>
#include <filesystem>
#include <locale>
#include <codecvt>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <bitset>

#define FMT_ENFORCE_COMPILE_STRING
#include <fmt/format.h>
#include <fmt/xchar.h>

#ifdef DO_MINMAX_REOVERRIDE
    #ifndef min
	    #define min(a,b)  (((a) < (b)) ? (a) : (b))
    #endif

    #ifndef max
	    #define max(a,b)  (((a) > (b)) ? (a) : (b))
    #endif
#endif

#ifdef GNUC
	#undef offsetof
	//#define offsetof( type, var ) __builtin_offsetof( type, var )
	#define offsetof(s,m)	(size_t)&(((s *)0)->m)
#else
	#undef offsetof
	#define offsetof(s,m)	(size_t)&(((s *)0)->m)
#endif

#ifdef DO_MEM_REOVERRIDE
    #include <memdbgon.h>
#endif

#endif
