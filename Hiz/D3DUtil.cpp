#include "DXUT.h"

#include "strsafe.h"
#include "D3DUtil.h"

#include <time.h>

using namespace D3DUtils;

// Class constructor. Takes the necessary information and
// composes a string that will appear in PIXfW.
ScopeProfiler::ScopeProfiler( WCHAR* Name, int Line )
{
    WCHAR wc[ MAX_PATH ];
    StringCchPrintf( wc, MAX_PATH, L"%s @ Line %d.\0", Name, Line );
    D3DPERF_BeginEvent( D3DCOLOR_XRGB( rand() % 255, rand() % 255, rand() % 255 ), wc );
    srand( static_cast< unsigned >( time( NULL ) ) );
}

// Makes sure that the BeginEvent() has a matching EndEvent()
// if used via the macro in D3DUtils.h this will be called when
// the variable goes out of scope.
ScopeProfiler::~ScopeProfiler( )
{
    D3DPERF_EndEvent( );
}