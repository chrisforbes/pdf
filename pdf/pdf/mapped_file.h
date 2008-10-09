#pragma once

class MappedFile
{
	HANDLE hFile;
	HANDLE hMapping;
	char const * f;
	size_t size;

public:
	MappedFile( wchar_t const * filename )
		: hFile( INVALID_HANDLE_VALUE ), hMapping( INVALID_HANDLE_VALUE ), f(0)
	{
		hFile = ::CreateFile( filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0 );
		if (hFile == INVALID_HANDLE_VALUE)
			return;

		DWORD sizeHigh;
		size = ::GetFileSize( hFile, &sizeHigh );

		if (sizeHigh)
			return;

		hMapping = ::CreateFileMapping( hFile, 0, PAGE_WRITECOPY, 0, size, 0 );
		if (hMapping == INVALID_HANDLE_VALUE)
			return;

		f = (char const *)::MapViewOfFile( hMapping, FILE_MAP_COPY, 0, 0, 0 );
	}

	bool IsValid() const
	{
		return f != 0;
	}

	char const * F() const
	{
		return f;
	}

	size_t Size() const 
	{
		return size;
	}

	~MappedFile()
	{
		if (f)
			::UnmapViewOfFile( f );
		if (hMapping != INVALID_HANDLE_VALUE)
			::CloseHandle( hMapping );
		if (hFile != INVALID_HANDLE_VALUE)
			::CloseHandle( hFile );
	}
};
