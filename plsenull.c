/* plsenull.c -- scans PL/SQL source code for attempts to test for
   equality (or inequality) to NULL.

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

static int plsenull( Sfile s );

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

	/* look for comparisons to null */

	rc = plsenull( s );

	s_close( &s );
	if( pIn != stdin )
		fclose( pIn );

	if( OKAY == rc )
		return EXIT_SUCCESS;
	else
		return EXIT_FAILURE;
}

/********************************************************************
 parse -- main loop looking for tests of equality, or inequality, to
 NULL.
 *******************************************************************/
static int plsenull( Sfile s )
{
	int rc = OKAY;
	Pls_token_type curr_type = T_none;
	Pls_token_type prev_type;
	Pls_tok * pT;

	do
	{
		pT = pls_next_tok( s );

		if( NULL == pT )
		{
			fprintf( stderr, "Memory exhausted!\n" );
			return ERROR_FOUND;
		}

		prev_type = curr_type;
		curr_type = pT->type;

		if( T_null == curr_type )
		{
			if( T_equals == prev_type )
			{
				fprintf( stderr, "Line %d, column %d: "
					"NULL following an equals sign\n",
					pT->line, pT->col );
				rc = ERROR_FOUND;
			}
			else if( T_not_equal == prev_type )
			{
				fprintf( stderr, "Line %d, column %d: "
					"NULL following a not-equal sign\n",
					pT->line, pT->col );
				rc = ERROR_FOUND;
			}
		}
		else if( T_equals == curr_type )
		{
			if( T_null == prev_type )
			{
				fprintf( stderr, "Line %d, column %d: "
					"Equals sign following NULL\n",
					pT->line, pT->col );
				rc = ERROR_FOUND;
			}
		}
		else if( T_not_equal == curr_type )
		{
			if( T_null == prev_type )
			{
				fprintf( stderr, "Line %d, column %d: "
					"Not-equal sign following NULL\n",
					pT->line, pT->col );
				rc = ERROR_FOUND;
			}
		}

		pls_free_tok( &pT );

	} while( curr_type != T_eof );

	return rc;
}
