#include "pch.h"

#include "textstate.h"

#define ZOOM(k) ((k) * zoom / 72)

static int saturate( int k )
{
	if( k <= 0 )
		return 0;
	if( k >= 255 )
		k = 255;
	return k;
}

void RenderImage( HDC intoDC, PStream image, const Matrix & ctm, int pageHeight, XrefTable const & xrefTable, int zoom )
{
	PObject colorSpace = image->dict->Get( "ColorSpace", xrefTable );
	PNumber bits = image->dict->Get<Number>( "BitsPerComponent", xrefTable );
	PNumber width = image->dict->Get<Number>( "Width", xrefTable );
	PNumber height = image->dict->Get<Number>( "Height", xrefTable );

	int w = width->num;
	int h = height->num;

	size_t length;
	const unsigned char* imageBytes = (const unsigned char*)image->GetStreamBytes( xrefTable, &length );

	unsigned* xRGB = new unsigned[ w*h ];

	if( length == w * h * 4 )
	{
		// TODO: implement ICC color hax
		//For now, assume it's CMYK
		for( int y = 0 ; y < h ; y++ )
		{
			for( int x = 0 ; x < w ; x++ )
			{
				int k = imageBytes[ (y*w+x)*4 + 3 ];

				int r = (255-k)*(255-imageBytes[ (y*w+x)*4 + 0 ]) / 255;
				int g = (255-k)*(255-imageBytes[ (y*w+x)*4 + 1 ]) / 255;
				int b = (255-k)*(255-imageBytes[ (y*w+x)*4 + 2 ]) / 255;

				xRGB[y*w+x] = (((r << 8) + g) << 8) + b
			}
		}
	}
	else if( length == w * h * 3 )
	{
		// assume RGB
		for( int y = 0 ; y < h ; y++ )
			for( int x = 0 ; x < w ; x++ )
				xRGB[y*w+x] = ((((imageBytes[(y*w+x)*3] << 8) + imageBytes[(y*w+x)*3+1]) << 8) + imageBytes[(y*w+x)*3+2]);
	}
	else
	{
		// o_O
	}

	BITMAPINFOHEADER bmi = { sizeof( BITMAPINFOHEADER ), w, -h, 1, 32, BI_RGB };

	StretchDIBits( intoDC, ZOOM(ctm.v[4]), ZOOM(pageHeight - ctm.v[5] - ctm.v[3]), ZOOM(ctm.v[0]), ZOOM(ctm.v[3]), 0, 0, w, h ,xRGB, (BITMAPINFO*)&bmi, DIB_RGB_COLORS, SRCCOPY );

	delete[] xRGB;
	image->EvictCache();
}
