/* plscount.c -- scans PL/SQL source code to count tokens; writes
   resulting count to standard output.

   Copyright (C) 2000  Scott McKellar  mck9@swbell.net

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

static int plscount( Sfile s, unsigned long *pCount );

int main( int argc, char * argv[] )
{
	int rc;
	FILE * pIn;
	Sfile s;
	unsigned long count;

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

	/* count tokens */

	rc = plscount( s, &count );

	if( OKAY == rc )
		printf( "%lu\n", count );

	s_close( &s );
	if( pIn != stdin )
		fclose( pIn );

	if( OKAY == rc )
		return EXIT_SUCCESS;
	else
		return EXIT_FAILURE;
}

/********************************************************************
 plscount -- main loop for counting tokens
 *******************************************************************/
static int plscount( Sfile s, unsigned long *pCount )
{
	int rc = OKAY;
	unsigned long count = 0L;
	Pls_token_type curr_type = T_none;
	Pls_tok * pT;

	do
	{
		pT = pls_next_tok( s );

		if( NULL == pT )
		{
			fprintf( stderr, "Memory exhausted!\n" );
			return ERROR_FOUND;
		}

		curr_type = pT->type;
		if( curr_type != T_eof )
			++count;

		pls_free_tok( &pT );

	} while( curr_type != T_eof );

	if( OKAY == rc )
		*pCount = count;

	return rc;
}
