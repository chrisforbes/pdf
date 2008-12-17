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

void RenderImage( HDC intoDC, HDC tmpDC, PStream image, const Matrix & ctm, int pageHeight, XrefTable const & xrefTable, int zoom )
{
	PObject colorSpace = image->dict->Get( "ColorSpace", xrefTable );
	PNumber bits = image->dict->Get<Number>( "BitsPerComponent", xrefTable );
	PNumber width = image->dict->Get<Number>( "Width", xrefTable );
	PNumber height = image->dict->Get<Number>( "Height", xrefTable );

	int w = width->num;
	int h = height->num;

	HBITMAP bitmap = CreateCompatibleBitmap( intoDC, width->num, height->num );
	HGDIOBJ old = SelectObject( tmpDC, bitmap );

	size_t length;
	const unsigned char* imageBytes = (const unsigned char*)image->GetStreamBytes( xrefTable, &length );

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

				SetPixel( tmpDC, x, y, (b * 0x100 + g) * 0x100 + r );
			}
		}
	}
	else if( length == w * h * 3 )
	{
		// assume RGB
		for( int y = 0 ; y < h ; y++ )
			for( int x = 0 ; x < w ; x++ )
				SetPixel( tmpDC, x, y, 0xffffff & *(COLORREF*)(imageBytes + (y*w+x)*3) );
	}
	else
	{
		// o_O
	}

	StretchBlt( intoDC, ZOOM(ctm.v[4]), ZOOM(pageHeight - ctm.v[5] - ctm.v[3]), ZOOM(ctm.v[0]), ZOOM(ctm.v[3]), tmpDC, 0, 0, w, h, SRCCOPY );

	SelectObject( tmpDC, old );
	DeleteObject( bitmap );
}
