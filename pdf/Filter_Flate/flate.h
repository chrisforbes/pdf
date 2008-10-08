#pragma once

	extern "C"
#ifdef FLATE_IMPL
	__declspec(dllexport)
#else
	__declspec(dllimport)
#endif
	char* Inflate( const char* inputStart, const char* inputEnd, size_t* length, void* (*alloc)( void*, size_t ) );