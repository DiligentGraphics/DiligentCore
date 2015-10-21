// File2Include.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "stdio.h"
#include "tchar.h"

int _tmain(int argc, _TCHAR* argv[])
{
    if( argc < 3 )
    {
        printf( "Incorrect number of command line arguments. Expected arguments: src file, dst file\n");
        return -1;
    }
    auto SrcFile = argv[1];
    auto DstFile = argv[2];
    FILE *pSrcFile = nullptr;
    if( _tfopen_s( &pSrcFile, SrcFile, _T( "r" ) ) != 0 )
    {
        _tprintf( _T("Failed to open source file %s\n"), SrcFile );
        return -1;
    }

    FILE *pDstFile = nullptr;
    if( _tfopen_s(&pDstFile, DstFile, _T( "w" ) ) != 0 )
    {
        _tprintf( _T("Failed to open destination file %s\n"), DstFile );
        fclose(pSrcFile);
        return -1;
    }


    _TCHAR Buff[1024];
    _TCHAR SpecialChars[] = _T( "\'\"\\" );
    while( !feof( pSrcFile ) )
    {
        auto Line = _fgetts( Buff, sizeof( Buff )/sizeof(Buff[0]) , pSrcFile );
        if( Line == nullptr )
            break;
        _fputtc( _T( '\"' ), pDstFile );
        auto CurrChar = Line;
        while( CurrChar && *CurrChar != '\n' )
        {
            if( _tcschr( SpecialChars, *CurrChar) )
                _fputtc( _T( '\\' ), pDstFile );
            _fputtc( *CurrChar, pDstFile );
            ++CurrChar;
        }
        _fputts( _T("\\n\"\n"), pDstFile );
    }

    fclose(pDstFile);
    fclose(pSrcFile);

    _tprintf( _T("File2String: sucessfully converted %s to %s\n"), SrcFile, DstFile );

	return 0;
}
