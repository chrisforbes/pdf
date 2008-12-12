#include "pch.h"
#include "parse.h"

// The "None" PNG predictor
static void Flate_None( char* dest, const char* src, int width )
{
	for( int i = 0 ; i < width ; i++ )
		dest[i] = src[i];
}

// The "Left" PNG predictor
static void Flate_Left( char* dest, const char* src, int width )
{
	char left = 0;
	for( int i = 0 ; i < width ; i++ )
		dest[i] = left = src[i] + left;
}

// The "Up" PNG predictor
static void Flate_Up( char* dest, const char* src, int width, const char* prevRow )
{
	if( prevRow )
		for( int i = 0 ; i < width ; i++ )
			dest[i] = src[i] + prevRow[i];
	else
		Flate_None( dest, src, width );
}

const char* Stream::ApplyFilter( const Name& filterName, PDictionary filterParms, const char* inputStart, const char* inputEnd, size_t * outputLength, XrefTable const & objmap )
{
	if( filterName.str == String( "FlateDecode" ) )
	{
		char* data = Inflate( inputStart, inputEnd, outputLength, realloc );

		if( !filterParms )
			return data;

		PNumber numCols = filterParms->Get<Number>( "Columns", objmap );
		int columns = numCols ? numCols->num : 1;

		PNumber numPredictor = filterParms->Get<Number>( "Predictor", objmap );
		int predictor = numPredictor ? numPredictor->num : 1;

		switch( predictor )
		{
		case 1:
			return data;
		case 10: // Any of these mean "use the png predictor". 
		case 11: // The predictor is chosen on a scanline-by-scanline basis.
		case 12:
		case 13:
		case 14:
			{
				// TODO: more parms: "Colors", "BitsPerComponent"
				const char* prev = NULL;
				const char* current = data;
				char* current_out = data;

				while( current < data + *outputLength )
				{
					switch( *current++ )
					{
					//case 0:
					//	Flate_None( current_out, current, columns );
					//	break;
					//case 1:
					//	Flate_Left( current_out, current, columns );
					//	break;
					case 2:
						Flate_Up( current_out, current, columns, prev );
						break;
					default:
						assert( !"win?" );
					}
					prev = current_out;
					current += columns;
					current_out += columns;
				}
				*outputLength = current_out - data;
				return data;
			}
		default:
			DebugBreak();
			return NULL;
		}
	}
	else
	{
		DebugBreak();
		return NULL;
	}
}
