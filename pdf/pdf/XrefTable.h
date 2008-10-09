#pragma once

struct Xref
{
	char const * ptr;
	size_t generation;
	mutable PObject cache;

	Xref( const char* ptr, int generation )
		: ptr( ptr ), generation( generation), cache()
	{
	}
};

class XrefTable
{
	struct XrefSection
	{
		int first;
		int count;
		const char* ptr;

		XrefSection( int first, int count, const char* ptr )
			: first( first ), count( count ), ptr( ptr )
		{
		}
	};

	std::vector<XrefSection> sections;
	typedef std::vector<XrefSection>::iterator sect_iter;
	typedef std::vector<XrefSection>::const_iterator sect_citer;

public:
	const Xref* find( int objectNum ) const
	{
		for( sect_citer it = sections.begin() ; it != sections.end() ; ++it )
		{
			if( objectNum >= it->first && objectNum < it->first + it->count )
				return (const Xref*)(it->ptr + 20 * (objectNum-it->first) );
		}

		return 0;
	}

	void addSection( int first, int count, const char* ptr )
	{
		sections.push_back( XrefSection( first, count, ptr ) );
	}

	void fillEntry( const char* p, const MappedFile& f )
	{
		Xref* xref = (Xref*)p;

		char* tokEnd = const_cast<char*>( p + 20 );
		size_t fileOffset = strtoul( p, &tokEnd, 10 );
		if( tokEnd != p + 10 )
			return;

		p += 11;
		tokEnd = const_cast<char*>( p + 9 );
		int generation = strtol( p, &tokEnd, 10 );
		if( tokEnd != p + 5 )
			return;

		new(xref) Xref( (p[6] != 'f') ? f.F() + fileOffset : 0, generation );
	}
};
