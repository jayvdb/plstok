/* plsb.c -- beautifier for PL/SQL source code, applying a consistent
   use of indentation and other white space.

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
#include "util.h"
#include "sfile.h"
#include "plstok.h"
#include "plsb.h"

static int beautify( void );

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

	rc = plsb_init();
	if( OKAY == rc )
		rc = plsb_open( s );

	if( OKAY == rc )
	{
		rc = beautify();
		s_close( &s );
	}

	if( pIn != stdin )
		fclose( pIn );

	if( OKAY == rc )
		return EXIT_SUCCESS;
	else
		return EXIT_FAILURE;
}

/********************************************************************
 parse -- main loop for fetching and displaying successive logical
 lines.
 *******************************************************************/
static int beautify( void )
{
	int rc = OKAY;
	int finished = FALSE;
	Toklist list;

	while( FALSE == finished )
	{
		rc = get_logical_line( &list );

		if( rc != OKAY )
		{
			fprintf( stderr, "Error getting logical line\n" );
			finished = TRUE;
		}
		else
		{
			rc = write_logical_line( &list );
			if( OKAY != rc )
			{
				fprintf( stderr, "Error writing logical line\n" );
				finished = TRUE;
			}
			else if( T_eof == list.pLast->type )
				finished = TRUE;
		}

		empty_toklist( &list );
	}

	return rc;
}
