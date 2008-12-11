#pragma once

struct Matrix
{
	double v[6];

	Matrix()
	{
		v[0] = 1; v[1] = 0; v[2] = 0; v[3] = 0; v[4] = 1; v[5] = 0;
	}
};

struct TextState
{
	double c;	// character spacing, in unscaled text units
	double w;	// word spacing (for ' ' only), in unscaled text units
	double h;	// horizontal scaling - in percent
	double l;	// leading, in unscaled text units
	double rise;	// current-glyph baseline adjustment
	int mode;
	double fontSize;
	char fontName[128];	// better not be bigger than this
	size_t fontNameLen;

	Matrix m, lm;

	TextState()
		: c(0), w(0), h(100), l(0), rise(0), mode(0), fontSize(0), m(), lm(), fontNameLen(0)
	{
	}

	double EffectiveFontHeight() const { return fontSize * lm.v[3]; }
	double EffectiveFontWidth() const { return fontSize * lm.v[0]; }

	double HorizontalScale() const { return h / 100.0; }
};

