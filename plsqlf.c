/* plsqlf.c -- scans PL/SQL source code for string or character literals
   which contain a line feed.

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

static char litbuf[ 501 ];

static int plsqlf( Sfile s );

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

	/* suppress white space and comments */

	(void) pls_nopreserve();

	/* look for literals containing line feeds */

	rc = plsqlf( s );

	s_close( &s );
	if( pIn != stdin )
		fclose( pIn );

	if( OKAY == rc )
		return EXIT_SUCCESS;
	else
		return EXIT_FAILURE;
}

/********************************************************************
 parse -- main loop looking for string or character literals which
 contain line feeds.

 We take a short cut here -- if a string literal has its first line
 feed only after 500 characters, we'll miss it.  That's a reasonable
 risk to take.  Few people will code anything that long on a single
 line.

 If you really want to be perfectionist you can use pls_tok_size()
 to determine the string length, and then dynamically allocate a
 sufficiently large buffer.
 *******************************************************************/
static int plsqlf( Sfile s )
{
	int rc = OKAY;
	Pls_token_type type;
	Pls_tok * pT;

	/* Examine each token */

	do
	{
		pT = pls_next_tok( s );

		if( NULL == pT )
		{
			fprintf( stderr, "Memory exhausted!\n" );
			return ERROR_FOUND;
		}

		type = pT->type;
		switch( type )
		{
			case T_string_lit :
				pls_copy_text( pT, litbuf, sizeof( litbuf ) );
				if( strchr( litbuf, '\n' ) != NULL )
				{
					fprintf( stderr, "Line %d, column %d: "
						"String literal containing line feed\n",
						pT->line, pT->col );
					rc = ERROR_FOUND;
				}
				break;
			case T_char_lit :
				if( '\n' == pT->buf[ 1 ] )
				{
					fprintf( stderr, "Line %d, column %d: "
						"Character literal containing line feed\n",
						pT->line, pT->col );
					rc = ERROR_FOUND;
				}
				break;
			default :
				break;
		}

		pls_free_tok( &pT );

	} while( type != T_eof );

	return rc;
}
