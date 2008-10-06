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
	String,
	Stream,
};
}

class Object;
typedef boost::shared_ptr<Object> PObject;

class Array;
typedef boost::shared_ptr<Array> PArray;
//class Bool;
class Dictionary;
typedef boost::shared_ptr<Dictionary> PDictionary;
class Name;
typedef boost::shared_ptr<Name> PName;
class Number;
typedef boost::shared_ptr<Number> PNumber;
//class Stream;
class String;
typedef boost::shared_ptr<String> PString;
class Indirect;
typedef boost::shared_ptr<Indirect> PIndirect;

struct Xref
{
	char const * ptr;
	size_t generation;

	Xref()
		: ptr(0), generation(0) {}
};

typedef std::map<size_t, Xref> XrefTable;

class Object
{
public:
	virtual ~Object() {}
	virtual ObjectType::ObjectType Type() const = 0;
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
	const char* start;
	const char* end;

public:
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

	void Add( const Name& key, const PObject& value )
	{
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
		XrefTable::const_iterator it = t.find( objectNum );
		if (it == t.end() || it->second.generation != generation)
			return 0;

		return it->second.ptr;
	}

	IMPLEMENT_OBJECT_TYPE( Ref );
};
