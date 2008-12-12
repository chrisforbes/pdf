#include "pch.h"
#include "parse.h"

static int ReadXrefStrNum( int size, const char*& stream, const char* streamEnd )
{
	assert( stream + size <= streamEnd );

	unsigned int ret = 0;

	assert( size <= 4 );

	for( int i = 0 ; i < size ; i++ )
		ret = (ret << 8u) + (unsigned)(unsigned char)*stream++;

	return (int)ret;
}

void XrefTable::ReadXrefStream( PStream xrefStream, const char* streamContent, size_t streamLength )
{
	const char* streamEnd = streamContent + streamLength;

	PNumber size = xrefStream->dict->Get<Number>( "Size", XrefTable( file ) );
	assert( size != NULL );
	PArray index = xrefStream->dict->Get<Array>( "Index", XrefTable( file ) );

	typedef std::vector<std::pair<size_t, size_t> > blocks_t;
	blocks_t streamBlocks;

	if( index == NULL )
		streamBlocks.push_back( std::pair< size_t, size_t >( 0, size->num ) );
	else
	{
		typedef std::vector<PObject>::const_iterator iter;
		iter n = index->elements.begin();
		while( n != index->elements.end() )
		{
			int start = boost::shared_dynamic_cast<Number>( *n )->num;
			++n;
			int num = boost::shared_dynamic_cast<Number>( *n )->num;
			++n;
			streamBlocks.push_back( std::pair< size_t, size_t >( start, num ) );
		}
	}

	PArray fieldSizes = xrefStream->dict->Get<Array>( "W", XrefTable( file ) );
	assert( fieldSizes );

	PNumber idSize = boost::shared_dynamic_cast< Number >( fieldSizes->elements[0] );
	PNumber objNumSize = boost::shared_dynamic_cast< Number >( fieldSizes->elements[1] );
	PNumber generationSize = boost::shared_dynamic_cast< Number >( fieldSizes->elements[2] );

	int idS = idSize->num;
	int objNumS = objNumSize->num;
	int genS = generationSize->num;

	//char sz[128];
	//_snprintf( sz, 128, "FieldSizes: %u  %u  %u", idS, objNumS, genS );
	//OutputDebugStringA( sz );

	for( blocks_t::const_iterator n = streamBlocks.begin() ; n != streamBlocks.end() ; n++ )
	{
		sections.push_back( XrefSection( n->first, n->second, n->second ) );
		const char* p = sections.back().ptr;
		for( size_t x = 0 ; x < n->second ; x++ )
		{
			int type = (idS == 0) ? 1 : ReadXrefStrNum( idS, streamContent, streamEnd );
			int obj = (objNumS == 0) ? 0 : ReadXrefStrNum( objNumS, streamContent, streamEnd );
			int gen = (genS == 0) ? 0 : ReadXrefStrNum( genS, streamContent, streamEnd );

			//char sz[256];
			//_snprintf( sz, 256, "Object %u:  %u  %x  %u\n", sections.back().first + x, type, obj, gen );
			//OutputDebugStringA( sz );

			switch( type )
			{
			case 0:
				construct( (void*)p, 0, 0, 0 );
				break;
			case 1:
				construct( (void*)p, obj, gen, 0 );
				break;
			case 2:
				construct( (void*)p, gen, 0, obj );
				break;
			default:
				assert( !"NOTI" );
				break;
			}

			p += 20;
		}
	}
}

PObject XrefTable::ReadFromObjectStream( PStream stream, int objNum ) const
{
	size_t dataLength;
	const char* data = stream->GetStreamBytes( *this, &dataLength );

	PNumber firstObjectOffset = stream->dict->Get<Number>( "First", *this );
	if( !firstObjectOffset )
		return PObject();

	const char* p = data;
	for(;;)
	{
		PNumber obj = boost::shared_dynamic_cast<Number>( Parse( p, data + dataLength ) );
		PNumber offset = boost::shared_dynamic_cast<Number>( Parse( p, data + dataLength ) );
		if( !obj || !offset )
			return PObject();

		if( obj->num == objNum )
		{
			const char* start = data + firstObjectOffset->num + offset->num;
			return Parse( start, data + dataLength );
		}
	}

	return PObject();
}
