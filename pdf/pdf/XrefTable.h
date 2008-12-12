#pragma once

// TODO: get a home
extern char const * GetPdfNextLine( MappedFile const & f, char const * p );


struct Xref
{
	int ptr;
	size_t generation;
	mutable PObject cache;
	int stream;

	Xref( int ptr, int generation, int stream )
		: ptr( ptr ), generation( generation), cache(), stream( stream )
	{
	}
};

typedef char Xref_MUST_FIT_IN_XREF_TABLE[ 21 - sizeof( Xref )];

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

		XrefSection( int first, int count, size_t numEntries )
			: first( first ), count( count ), ptr( new char[ 20 * numEntries ] )
		{
		}
	};

	MappedFile const & file;
	std::vector<XrefSection> sections;
	typedef std::vector<XrefSection>::iterator sect_iter;
	typedef std::vector<XrefSection>::const_iterator sect_citer;

public:
	explicit XrefTable( MappedFile const & file )
		: file( file )
	{
	}

	const Xref* find( int objectNum ) const
	{
		for( sect_citer it = sections.begin() ; it != sections.end() ; ++it )
		{
			if( objectNum >= it->first && objectNum < it->first + it->count )
				return (const Xref*)(it->ptr + 20 * (objectNum-it->first) );
		}

		MessageBoxA( 0, "(in XrefTable): Looking for nonexistant object", "Debug", 0 );
		return 0;
	}

	template< typename T >
	boost::shared_ptr<T> get( Indirect* indirect ) const
	{
		extern PObject ParseIndirect( const char* p, Indirect* i, const XrefTable& objmap );

		const Xref* xref = find( indirect->objectNum );
		if( !xref || xref->generation != indirect->generation )
			return boost::shared_ptr<T>();

		if( xref->stream == 0 && xref->ptr == 0 )
			return boost::shared_ptr<T>();

		if( !xref->cache )
		{
			if( xref->stream )
			{
				Indirect indirectStream( xref->stream, 0 );
				PStream stream = get<Stream>( &indirectStream );

				xref->cache = ReadFromObjectStream( stream, indirect->objectNum );
			}
			else
				xref->cache = ParseIndirect( file.F() + xref->ptr, indirect, *this );
		}

		return boost::shared_dynamic_cast<T>( xref->cache );
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

		construct( (void*)xref, (p[6] != 'f') ? fileOffset : 0, generation, 0 );
	}

	void construct( void* ptr, int fileOffset, int generation, int stream )
	{
		new(ptr) Xref( fileOffset, generation, stream );
	}

	const char* ReadXrefSubsection( const char* p )
	{
		assert( sizeof( Xref ) <= 20 );
		const char* q = GetPdfNextLine( file, p );

		char* tokEnd = const_cast<char*>( q );
		size_t first = strtoul( p, &tokEnd, 10 );
		if( !tokEnd || tokEnd == p )
			return 0;

		p = tokEnd;
		while( *p == ' ' )
			++p;

		tokEnd = const_cast<char*>( q );
		size_t count = strtoul( p, &tokEnd, 10 );
		if( !tokEnd || tokEnd == p )
			return 0;

		p = q;

		addSection( first, count, p );

		size_t end = first + count;

		for( ; first < end ; first++ )
		{
			fillEntry( p, file );

			p += 20;
		}
		return p;
	}

	char const* ReadXrefSection( char const * p )
	{
		p = GetPdfNextLine( file, p );

		while( p && ::memcmp( p, "trailer", 7 ) != 0 )
			p = ReadXrefSubsection( p );

		return p;
	}

	void ReadXrefStream( PStream xrefStream, const char* streamContent, size_t streamLength );

	PObject ReadFromObjectStream( PStream stream, int objNum ) const;
};
