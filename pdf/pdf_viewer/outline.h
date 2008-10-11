#pragma once

void BuildOutline( Dictionary * parent, HTREEITEM parentItem, const XrefTable & objmap );
void ExpandNode( Document * doc, NMTREEVIEW * info );