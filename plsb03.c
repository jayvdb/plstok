/* plsb03.c -- routines for writing a logical line

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

static int deferred_unindents = 0;

/* The following is used to maintain a stack to keep track of  */
/* which token types are at the beginning of each level of     */
/* indent.  The main reason is so that we can un-indent twice  */
/* between the end of a WHEN clause and a following END token. */

#define TYPESTACK_DEPTH 32
static Pls_token_type typestack[ TYPESTACK_DEPTH ];
static int type_top = -1;

static int sometimes_indent( Pls_token_type first, Pls_token_type second );
static void put_line( Toklist * pTL );
static void write_indent( int i );

static Pls_token_type pop_type( void );
static void push_type( Pls_token_type type );

/*******************************************************************
 defer_unindent -- Note that at the beginning of the next logical
 line we should unindent a specified number of times.
 ******************************************************************/
void defer_unindent( int how_many )
{
	deferred_unindents = how_many;
}

/*******************************************************************
 write_logical_line -- Write the tokens of a logical line in a nicely
 formatted manner.  This task entails:

 1. Putting or not putting a blank space between tokens

 2. Breaking the logical line into two or more physical lines in
	order to avoid exceeding a maximum line length (not implemented)
 ********************************************************************/
int write_logical_line( Toklist * pTL )
{
	int rc = OKAY;
	Probability if_indent;
	Pls_token_type last_type;
	Pls_token_type first_type;
	Toknode * pTN;
	Toknode * pNext;

	/* sanity checks */

	ASSERT( pTL != NULL );
	if( NULL == pTL )
		return ERROR_FOUND;

	pTN = pTL->pFirst;
	ASSERT( pTN != NULL );
	ASSERT( pTN->pT != NULL );
	if( NULL == pTN || NULL == pTN->pT ) /* nothing to write */
		return ERROR_FOUND;
	else if( T_eof == pTN->type )
		return OKAY;

	first_type = pTN->type;
	last_type  = pTL->pLast->type;

	/* Determine where to put separator spaces */

	for( pNext = pTN->pNext; pNext != NULL;
		 pTN = pNext, pNext = pNext->pNext )
	{
		if( need_space( pTN->type, pNext->type ) == TRUE )
			pNext->spacer = 1;
	}

	/* Decrease the degree of indentation if necessary.  This is  */
	/* tricky when leaving an EXCEPTION block because you have to */
	/* unindent twice.  That's why we need a stack -- to keep     */
	/* track of what kind of token started the current indent.    */

	if( need_unindent( first_type ) )
	{
		Pls_token_type indent_type;

		if( pls_globals.indentation >= 0 )
			--pls_globals.indentation;
		indent_type = pop_type();
		if( T_when == indent_type &&
			T_end  == first_type  &&
			pls_globals.indentation >= 0 )
			--pls_globals.indentation;
	}

	/* Analyze the syntax; add additional   */
	/* line feeds and indentation as needed */

	rc = edit_syntax( pTL );

	/* Write the tokens */

	put_line( pTL );

	/* Write a newline -- unless we just wrote a "--" - style */
	/* comment, since that has its own newline. */

	if( last_type != T_remark ||
		pTL->pLast->pT->buf[ 0 ] != '-' )
		fputc( '\n', stdout );

	/* Adjust indentation if necessary */

	if_indent = need_indent( first_type );
	if( P_sometimes == if_indent )
	{
		/* look at the second token and then */
		/* decide whether you need to indent */

		Toknode * pSecond;
		Pls_token_type second_type;

		pSecond = pTL->pFirst->pNext;
		if( NULL == pSecond )
			second_type = T_none;
		else
			second_type = pSecond->type;

		if( sometimes_indent( first_type, second_type ) == TRUE )
			if_indent = P_always;
	}

	if( P_always == if_indent )
	{
		++pls_globals.indentation;
		push_type( first_type );
		if( T_exception == first_type )
			++pls_globals.indentation;
	}

	pls_globals.indentation -= deferred_unindents;
	if( pls_globals.indentation < 0 )
		pls_globals.indentation = 0;
	deferred_unindents = 0;

	return rc;
}

/********************************************************************
 sometimes_indent -- decide whether the first token of a line calls
 for subsequent indentation, depending on the second token
 *******************************************************************/
static int sometimes_indent( Pls_token_type first, Pls_token_type second )
{
	switch( first )
	{
	case T_for :
		if( T_update == second )
			return FALSE;
		else
			return TRUE;
	default :

		/* Not sure here; value returned is arbitrary */

		return FALSE;
	}
}

/********************************************************************
 put_line -- write the indentation and the tokens
 *******************************************************************/
static void put_line( Toklist * pTL )
{
	Toknode * pTN;

	/* Write each token, preceded by a white space as needed */

	for( pTN = pTL->pFirst; pTN != NULL; pTN = pTN->pNext )
	{
		/* Write a line feed if need be (except that if we are */
		/* on the first token of the line, we have already     */
		/* written one). */

		if( pTN != pTL->pFirst && TRUE == pTN->lf )
			fputc( '\n', stdout );

		pls_globals.indentation += pTN->indent_change;

		/* Write any necessary indentation */

		if( pTN == pTL->pFirst || TRUE == pTN->lf )
			write_indent( pls_globals.indentation );
		else
			if( pTN->spacer > 0 )
				fputc( ' ', stdout );

		pls_write_text( pTN->pT, stdout );
	}
}

/********************************************************************
 write_indent -- write the specified degree of indentation
 *******************************************************************/
static void write_indent( int i )
{
	for( ; i > 0; --i )
		fputs( pls_globals.indent_string, stdout );
}

/********************************************************************
 push_type -- push a type on the type stack.
 *******************************************************************/
static void push_type( Pls_token_type type )
{
	++type_top;
	if( type_top < TYPESTACK_DEPTH )
		typestack[ type_top ] = type;
}

/*******************************************************************
 pop_type -- pop a type from the type_stack.
 ******************************************************************/
static Pls_token_type pop_type( void )
{
	Pls_token_type type;

	if( type_top >= 0 )
	{
		if( type_top < TYPESTACK_DEPTH )
			type = typestack[ type_top ];
		else
			type = T_none;

		--type_top;
	}
	else
		type = T_none;

	return type;
}