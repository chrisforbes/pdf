#pragma once

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

class Object
{
	//TODO: type-test fns so we don't need rtti.
};

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

	bool operator==( const String& other ) const
	{
		if( end-start != other.end-other.start )
			return false;

		memcmp( start, other.start, end-start );
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
};

class Name : public Object
{
public:
	const String str;

	Name( const String& str )
		: str( str )
	{
	}
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

	void Add( const Name& key, const PObject& value )
	{
		dict[ key.str ] = value;
	}
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
};
