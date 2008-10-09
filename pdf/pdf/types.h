#pragma once

namespace ObjectType
{
enum ObjectType
{
	Array,
	Dictionary,
	Ref,
	Name,
	Bool,
	Number,
	Double,
	String,
	Stream,
};
}

class Object;
typedef boost::shared_ptr<Object> PObject;

class Array;
typedef boost::shared_ptr<Array> PArray;
class Bool;
typedef boost::shared_ptr<Bool> PBool;
class Dictionary;
typedef boost::shared_ptr<Dictionary> PDictionary;
class Name;
typedef boost::shared_ptr<Name> PName;
class Number;
typedef boost::shared_ptr<Number> PNumber;
class Double;
typedef boost::shared_ptr<Double> PDouble;
class Stream;
typedef boost::shared_ptr<Stream> PStream;
class String;
typedef boost::shared_ptr<String> PString;
class Indirect;
typedef boost::shared_ptr<Indirect> PIndirect;

struct Xref
{
	char const * ptr;
	size_t generation;
	mutable PObject cache;

	Xref()
		: ptr(0), generation(0) {}
};

typedef std::vector<Xref> XrefTable;

class Object
{
public:
	virtual ~Object() {}
	virtual ObjectType::ObjectType Type() const = 0;

	static PObject ResolveIndirect_( PObject p, const XrefTable & t );
};

#define IMPLEMENT_OBJECT_TYPE( t )\
	ObjectType::ObjectType Type() const { return ObjectType::t; }

class Array : public Object
{
public:
	std::vector<PObject> elements;

	Array()
	{
	}

	void Add( const PObject& obj )
	{
		elements.push_back( obj );
	}

	IMPLEMENT_OBJECT_TYPE( Array );
};

class String : public Object
{
public:
	const char* const start;
	const char* const end;

	String( const char* start, const char* end )
		: start( start ), end( end )
	{
	}

	String( char const * literalString )
		: start( literalString ), end( literalString + strlen( literalString ) )
	{}

	bool operator==( const String& other ) const
	{
		if( end-start != other.end-other.start )
			return false;

		return 0 == memcmp( start, other.start, end-start );
	}

	bool operator<( const String& other ) const
	{
		const char* a = start;
		const char* b = other.start;

		if( start == other.start && end == other.end )
			return true;

		for(;;)
		{
			if( b >= other.end )
				return false;
			if( a >= end )
				return true;
			int diff = *a - *b;
			if( diff < 0 )
				return true;
			if( diff > 0 )
				return false;
			++a;
			++b;
		}
	}

	size_t Length() const { return end - start; }

	IMPLEMENT_OBJECT_TYPE( String );
};

class Name : public Object
{
public:
	const String str;

	Name( const String& str )
		: str( str )
	{
	}

	IMPLEMENT_OBJECT_TYPE( Name );
};

class Number : public Object
{
public:
	int num;

	Number( const char* start, const char* end )
	{
		char* e = const_cast<char*>( end );
		num = strtol( start, &e, 10 );
		assert( e == end );
	}

	Number( const Number& other )
		: num( other.num )
	{
	}

	IMPLEMENT_OBJECT_TYPE( Number );
};

class Double : public Object
{
public:
	double num;

	Double( char const * start, char const * end )
	{
		char * e = const_cast< char * >( end );
		num = strtod( start, &e );
		assert( e == end );
	}

	Double( Double const & other )
		: num( other.num )
	{
	}

	IMPLEMENT_OBJECT_TYPE( Double );
};

class Dictionary : public Object
{
	std::map<String, PObject> dict;
	typedef std::map<String, PObject>::const_iterator dict_it;
public:
	PObject Get( const Name& name )
	{
		dict_it it = dict.find( name.str );
		if( it != dict.end() )
			return it->second;
		return PObject();
	}

	PObject Get( char const * literalString )
	{
		return Get( Name( String( literalString ) ) );
	}

	PObject Get( char const * literalString, const XrefTable& objmap )
	{
		return Object::ResolveIndirect_( Get( literalString ), objmap );
	}

	template< typename T >
	boost::shared_ptr<T> Get( char const * literalString, const XrefTable& objmap )
	{
		// TODO: do this without the dynamic_cast
		return boost::shared_dynamic_cast<T>( Get( literalString , objmap ) );
	}

	void Add( const Name& key, const PObject& value )
	{
		if( !value )
			DebugBreak();

		dict[ key.str ] = value;
	}

	IMPLEMENT_OBJECT_TYPE( Dictionary );
};

class Indirect : public Object
{
public:
	int objectNum;
	int generation;

	Indirect( const Number& objectNum, const Number& generation )
		: objectNum( objectNum.num ), generation( generation.num )
	{
	}

	char const * Resolve( XrefTable const & t )
	{
		if( (size_t)objectNum >= t.size() )
			return 0;

		const Xref& xref = t[ objectNum ];

		if( xref.generation != generation )
			return 0;

		return xref.ptr;
	}

	IMPLEMENT_OBJECT_TYPE( Ref );
};

class Bool : public Object
{
public:
	const bool value;
	Bool( bool value )
		: value( value )
	{
	}

	IMPLEMENT_OBJECT_TYPE( Bool );
};

class Stream : public Object
{
public:
	PDictionary dict;
	const char* start;
	const char* end;

	const char* decodedStart;
	bool decodedIsAllocated;

	Stream( const PDictionary& dict, const char* start, const char* end )
		: dict( dict ), start( start ), end( end ), decodedStart( 0 ), decodedIsAllocated( false )
	{
	}

	const char* GetStreamBytes( const XrefTable & objmap )
	{
		if( decodedStart )
			return decodedStart;
		PObject filter = dict->Get( "Filter", objmap );
		if( !filter )
			return decodedStart = start;

		if( filter->Type() == ObjectType::Name )
			decodedStart = ApplyFilter( *boost::shared_static_cast<Name>( filter ), start, end );
		else
			DebugBreak();

		return decodedStart;
	}

	const char* ApplyFilter( const Name& filterName, const char* inputStart, const char* inputEnd );

	~Stream()
	{
		if( decodedIsAllocated )
			free( (void*)decodedStart );
	}
	IMPLEMENT_OBJECT_TYPE( Stream );
};

class DoubleRect
{
public:
	double left, top, right, bottom;

	DoubleRect()
		: left(0), top(0), right(0), bottom(0) {}

	DoubleRect( double left, double top, double right, double bottom )
		: left(left), top(top), right(right), bottom(bottom) {}
};