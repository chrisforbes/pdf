#include "pch.h"

void AdjustControlPlacement( HWND parent, HWND child, WINDOWPOS * p, RECT controlAlignment )
{
	// DAMN this thing is crazy

	RECT wndRect, clientRect;
	::GetWindowRect( parent, &wndRect );
	::GetClientRect( parent, &clientRect );

	if (p)
	{
		clientRect.right += p->cx - (wndRect.right - wndRect.left);
		clientRect.bottom += p->cy - (wndRect.bottom - wndRect.top);
	}

	if (controlAlignment.left < 0) controlAlignment.left = clientRect.right - clientRect.left - controlAlignment.left;
	if (controlAlignment.right <= 0) controlAlignment.left = clientRect.right - clientRect.left - controlAlignment.right;

	if (controlAlignment.top < 0) controlAlignment.top = clientRect.bottom - clientRect.top - controlAlignment.top;
	if (controlAlignment.bottom <= 0) controlAlignment.bottom = clientRect.bottom - clientRect.top - controlAlignment.bottom;

	::SetWindowPos( child, 0, controlAlignment.left, controlAlignment.top, 
		controlAlignment.right - controlAlignment.left,
		controlAlignment.bottom - controlAlignment.top,
		SWP_NOZORDER );
}