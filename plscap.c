/* plscap.c -- a filter for PL/SQL source code: raises reserved words
   to upper case, lowers identifiers to lower case.

    Copyright (C) 1999  Scott McKellar  mck9@swbell.net

    This program is open software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "util.h"
#include "sfile.h"
#include "plstok.h"

static int capitalize( Sfile s );
static void str_tolower( char * s );

int main( int argc, char * argv[] )
{
	int rc;
	FILE * pIn;
	Sfile s;

	if( argc < 2 )
		pIn = stdin;
	else
	{
		pIn = fopen( argv[ 1 ], "r" );
		if( NULL == pIn )
		{
			fprintf( stderr, "Unable to open %s for input\n",
				argv[ 1 ] );
			return EXIT_FAILURE;
		}
	}

	s = s_assign( pIn );
	if( NULL == s.p )
	{
		fprintf( stderr, "Unable to assign an Sfile\n" );
		if( pIn != stdin )
			fclose( pIn );
		return EXIT_FAILURE;
	}

	rc = capitalize( s );

	s_close( &s );
	if( pIn != stdin )
		fclose( pIn );

	if( OKAY == rc )
		return EXIT_SUCCESS;
	else
		return EXIT_FAILURE;
}

/********************************************************************
 parse -- main loop for capitalizing PL/SQL.
 *******************************************************************/
static int capitalize( Sfile s )
{
	int rc = OKAY;
	int finished = FALSE;
	Pls_tok * pT;

	while( FALSE == finished )
	{
		pT = pls_next_tok( s );

		if( NULL == pT )
		{
			fprintf( stderr, "Memory exhausted!\n" );
			rc = ERROR_FOUND;
			finished = TRUE;
		}
		else
		{
			if( T_identifier == pT->type )
			{
				str_tolower( pT->buf );
				pls_write_text( pT, stdout );
			}
			else if( T_eof == pT->type )
				finished = TRUE;
			else
			{
				const char * pKeyword;

				/* Here we rely on the fact that pls_keyword_name   */
				/* returns a pointer to a string in all upper case. */

				pKeyword = pls_keyword_name( pT->type );
				if( '\0' == pKeyword[ 0 ] )
					pls_write_text( pT, stdout );
				else
					fputs( pKeyword, stdout );

				if( T_error == pT->type )
				{
					fflush( stdout );
					fprintf( stderr, "\nERROR at line %d, column %d: %s\n",
						pT->line, pT->col, pT->msg );
					fflush( stderr );
				}
			}

			pls_free_tok( &pT );
		}
	}

	return rc;
}

/**********************************************************************
 str_tolower -- reduce all characters in a string to lower case.
 *********************************************************************/
static void str_tolower( char * s )
{
	ASSERT( s != NULL );

	if( s != NULL )
	{
		for( ; *s != NULL; ++s )
			*s = tolower( (unsigned char) *s );
	}
}