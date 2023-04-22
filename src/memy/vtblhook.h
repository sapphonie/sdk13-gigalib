#ifndef MEMY_VTBL_HOOK_H
#define MEMY_VTBL_HOOK_H
#ifdef _WIN32
#pragma once
#endif

#include "memytools.h"

/* ======== SourceHook ========
* Copyright (C) 2004-2010 Metamod:Source Development Team
* No warranties of any kind
*
* License: zlib/libpng
*
* Author(s): Pavol "PM OnoTo" Marko
* ============================
*/
template<typename FUNC_TYPE>
int GetFunctionOffset( FUNC_TYPE func )
{
#if defined(GNUC)
	union
	{
		FUNC_TYPE ptr;
		int delta;
	};
	ptr = func;

	if ( delta & 1 )
		return ( delta - 1 ) / sizeof( void * );

	return delta / sizeof( void * );
#elif defined(_MSC_VER)
	union
	{
		FUNC_TYPE ptr;
		byte *addr;
	};
	ptr = func;

	if ( *addr == 0xE9 )
	{
		addr += 5 + *(ulong *)( addr + 1 );
	}

	bool ok = false;
	if ( addr[0] == 0x8B && addr[1] == 0x44 && addr[2] == 0x24 && addr[3] == 0x04 &&
		 addr[4] == 0x8B && addr[5] == 0x00 )
	{
		addr += 6;
		ok = true;
	}
	else if ( addr[0] == 0x8B && addr[1] == 0x01 )
	{
		addr += 2;
		ok = true;
	}
	else if ( addr[0] == 0x48 && addr[1] == 0x8B && addr[2] == 0x01 )
	{
		addr += 3;
		ok = true;
	}
	if ( !ok )
		return -1;

	if ( *addr++ == 0xFF )
	{
		if ( *addr == 0x60 )
		{
			return *++addr / sizeof( void * );
		}
		else if ( *addr == 0xA0 )
		{
			return *( (uint *)++addr ) / sizeof( void * );
		}
		else if ( *addr == 0x20 )
			return 0;
		else
			return -1;
	}
	return -1;
#endif
}

template<typename F, typename H>
F HookVTable( void *pObject, F pFunc, H hook )
{
	void **vtbl = *(void ***)pObject;
	int index = GetFunctionOffset( pFunc );
	void **vtbl_func = (void **)vtbl[index];

	memy::SetMemoryProtection( vtbl_func, sizeof( void * ), MEM_READ|MEM_WRITE|MEM_EXEC );

	F original = *(F *)vtbl_func;
	*vtbl_func = *(void **)&hook;

	memy::SetMemoryProtection( vtbl_func, sizeof( void * ), MEM_READ|MEM_EXEC );

	return original;
}

#endif