/* ttok.c -- test driver for plstok, a PL/SQL tokenizer.  This program is
   not polished, nor is it likely to be useful for anything but testing
   and debugging plstok.

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

static void show_token( const Pls_tok * pT );
static int parse( Sfile s, FILE * pOut );

int main( int argc, char * argv[] )
{
	int rc;
	FILE * pIn;
	FILE * pOut = NULL;
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

	pOut = fopen( "copy.txt", "w" );
	if( NULL == pOut )
		fprintf( stderr, "Unable to open copy file\n" );

	/* Uncomment the following as desired: */
	/* (void) pls_nopreserve(); */

	rc = parse( s, pOut );

	if( pOut != NULL )
		fclose( pOut );

	s_close( &s );
	if( pIn != stdin )
		fclose( pIn );

	if( OKAY == rc )
		return EXIT_SUCCESS;
	else
		return EXIT_FAILURE;
}

/********************************************************************
 parse -- main loop for fetching and displaying successive tokens.
 *******************************************************************/
static int parse( Sfile s, FILE * pOut )
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
			if( T_eof == pT->type )
				finished = TRUE;
			else
			{
				if( pOut != NULL )
					pls_write_text( pT, pOut );
				show_token( pT );
			}

			pls_free_tok( &pT );
		}
	}

	return rc;
}

/********************************************************************
 show_token -- display the type and, if appropriate, the textual
 contents of a token.
 ********************************************************************/
static void show_token( const Pls_tok * pT )
{
	if( NULL == pT )
		return;

	printf( "%4.4d\t%4.4d\t", pT->line, pT->col );

	switch ( pT->type )
	{
		case T_identifier :
			printf( "identifier '%s'\n", pT->buf );
			break;
		case T_quoted_id  :
			printf( "quoted identifier: " );
			pls_write_text( pT, stdout );
			printf( "\n" );
			break;
		case T_string_lit :
			printf( "string literal: " );
			pls_write_text( pT, stdout );
			printf( "\n" );
			break;
		case T_char_lit :
			printf( "character literal: " );
			pls_write_text( pT, stdout );
			printf( "\n" );
			break;
		case T_num_lit :
			printf( "numeric literal: " );
			pls_write_text( pT, stdout );
			printf( "\n" );
			break;
		case T_remark :
			printf( "comment: " );
			pls_write_text( pT, stdout );
			printf( "\n" );
			printf( "(Length = %lu)\n",
				(unsigned long) pls_tok_size( pT ) );
			break;
		case T_whitespace :
			printf( "whitespace: '" );
			pls_write_text( pT, stdout );
			printf( "'\n" );
			break;
		case T_plus :
			printf( "plus\n" );
			break;
		case T_minus_sign :
			printf( "minus\n" );
			break;
		case T_star :
			printf( "asterisk\n" );
			break;
		case T_virgule :
			printf( "virgule\n" );
			break;
		case T_equals :
			printf( "equals\n" );
			break;
		case T_less :
			printf( "less-than\n" );
			break;
		case T_greater :
			printf( "greater\n" );
			break;
		case T_lparens :
			printf( "left parenthesis\n" );
			break;
		case T_rparens :
			printf( "right parenthesis\n" );
			break;
		case T_semicolon :
			printf( "semicolon\n" );
			break;
		case T_percent :
			printf( "percent\n" );
			break;
		case T_comma :
			printf( "comma\n" );
			break;
		case T_dot :
			printf( "dot\n" );
			break;
		case T_at_sign :
			printf( "at sign\n" );
			break;
		case T_colon :
			printf( "colon\n" );
			break;
		case T_expo :
			printf( "exponentiation\n" );
			break;
		case T_not_equal :
			printf( "not-equal\n" );
			break;
		case T_tilde :
			printf( "tilde-equal\n" );
			break;
		case T_hat :
			printf( "circumflex-equal\n" );
			break;
		case T_less_equal :
			printf( "less-or-equal\n" );
			break;
		case T_greater_equal :
			printf( "greater-or-equal\n" );
			break;
		case T_assignment :
			printf( "assignment\n" );
			break;
		case T_arrow :
			printf( "arrow\n" );
			break;
		case T_range_dots :
			printf( "range\n" );
			break;
		case T_bars :
			printf( "concatenation\n" );
			break;
		case T_left_label :
			printf( "begin label\n" );
			break;
		case T_right_label :
			printf( "end label\n" );
			break;
		case T_none :
			printf( "internal error: undefined token\n" );
			break;
		case T_error :
			printf( "error: " );
			fputs( pT->msg, stdout );
			putc( '\n', stdout );
			break;
		default :
			if( pls_is_keyword( pT->type ) )
				printf( "reserved word: %s\n", pls_keyword_name( pT->type) );
			else
				printf( "invalid token\n" );
			break;
	}
}