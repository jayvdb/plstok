/* sfile.c -- implementation of Sfile functions to encapsulate source */
/* text for a compiler, etc.

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

#define STACK_SIZE 10

/* The public interface Sfile contains only an opaque pointer.  Within
   this source file we use that pointer to point to the following:
*/
typedef struct
{
	FILE * pF;
	void * generic_ptr;
	SF_func func;
	int line;
	int col;
	int prev_line;
	int prev_col;
	int closable;
	unsigned ungotten;
	int stack[ STACK_SIZE ];	/* int, not char; can store EOF */
} SF;

/****************************************************************
 s_open: open an Sfile from a specified file
 ***************************************************************/
Sfile s_open( const char * filename )
{
	Sfile s;
	SF * pS = NULL;
	FILE * pF;

	ASSERT( filename != NULL );

	if( filename != NULL )
	{
		pF = fopen( filename, "r" );
		if( pF != NULL )
		{
			pS = allocMemory( sizeof( SF ) );
			if( pS != NULL )
			{
				pS->pF = pF;
				pS->generic_ptr = NULL;
				pS->func = NULL;
				pS->line = 1;
				pS->col  = 1;
				pS->prev_line = 0;
				pS->prev_col  = 0;
				pS->closable = TRUE;
				pS->ungotten = 0;
			}
		}
	}
	s.p = pS;
	return s;
}

/****************************************************************
 s_assign: open an Sfile from an already-opened file
 ***************************************************************/
Sfile s_assign( FILE * pF )
{
	Sfile s;
	SF * pS;

	ASSERT( pF != NULL );

	if( NULL == pF )
		pS = NULL;
	else
	{
		pS = allocMemory( sizeof( SF ) );
		if( pS != NULL )
		{
			pS->pF = pF;
			pS->generic_ptr = NULL;
			pS->func = NULL;
			pS->line = 1;
			pS->col  = 1;
			pS->prev_line = 0;
			pS->prev_col  = 0;
			pS->closable = FALSE;
			pS->ungotten = 0;
		}
	}
	s.p = pS;
	return s;
}

/****************************************************************
 s_callback: open an Sfile by installing a callback function to fetch
 characters.  The void pointer will be passed back to the callback
 function, thus enabling the client code to specify whatever it
 needs to specify.
 ***************************************************************/
Sfile s_callback( SF_func func, void * p )
{
	Sfile s;
	SF * pS;

	ASSERT( func != NULL );
	if( NULL == func )
	{
		s.p = NULL;
		return s;
	}

	pS = allocMemory( sizeof( SF ) );
	if( pS != NULL )
	{
		pS->pF = NULL;
		pS->generic_ptr = p;
		pS->func = func;
		pS->line = 1;
		pS->col  = 1;
		pS->prev_line = 0;
		pS->prev_col  = 0;
		pS->closable = FALSE;
		pS->ungotten = 0;
	}
	s.p = pS;
	return s;
}

/****************************************************************
 s_close: if we've been reading a file which we opened ourselves,
 close it.  Then release all other associated resources.
 **************************************************************/
void s_close( Sfile * pS )
{
	ASSERT( pS != NULL );

	if( pS->p != NULL )
	{
		SF * pSF;

		pSF = pS->p;
		pS->p = NULL;

		if( pSF->pF != NULL && TRUE == pSF->closable )
			fclose( pSF->pF );

		freeMemory( pSF );
	}
}

/****************************************************************
 s_getc: fetch the next character, either from the stack, or from
 a file, or from a callback function.
 ***************************************************************/
int s_getc( Sfile s )
{
	SF * pS;
	int c;

	pS = s.p;
	ASSERT( pS != NULL );
	if( NULL == pS )
		return EOF;

	pS->prev_line = pS->line;
	pS->prev_col  = pS->col;

	if( pS->ungotten )
	{
		/* return a previously ungotten character */

		--pS->ungotten;
		c = pS->stack[ pS->ungotten ];
	}
	else
	{
		if( pS->func != NULL )
			c = pS->func( pS->generic_ptr );
		else
			c = fgetc( pS->pF );

		/* We don't increment the line number if we're re-fetching */
		/* something previously ungotten.  That's because we don't */
		/* decrement the line number when we unget something.      */

		if( '\n' == c )
		{
			++pS->line;
			pS->col = 0;
		}
	}

	if( EOF != c )
		++pS->col;

	return c;
}

/****************************************************************
 s_ungetc: put a specified character (up to STACK_SIZE) back into
 the input stream to be re-fetched later.  This is similar to
 ungetc() except that it lets you unget EOF (or any arbitrary
 int value, if you insist on abusing it).

 Note: if you unget past the beginning of the current line, the
 column number becomes negative.  There's no very good way to keep
 track of the line and column in this case, so things may get a
 little weird.  We try at least to be plausible and consistent.
 ***************************************************************/
int s_ungetc( Sfile s, int c )
{
	SF * pS;

	pS = s.p;
	ASSERT( pS != NULL );
	if( NULL == pS )
		return ERROR_FOUND;

	if( pS->ungotten >= STACK_SIZE )
		return ERROR_FOUND;

	pS->stack[ pS->ungotten ] = c;
	++pS->ungotten;
	--pS->col;
	pS->prev_line = pS->line;
	pS->prev_col  = pS->col - 1;
	return OKAY;
}

/********************************************************************
 s_position -- return the position of the character previously fetched
 *******************************************************************/
Sposition s_position( Sfile s )
{
	SF * pS;
	Sposition pos;

	pS = s.p;
	if( NULL == pS )
	{
		pos.line = 0;
		pos.col  = 0;
	}
	else
	{
		pos.line = pS->prev_line;
		pos.col  = pS->prev_col;
	}
	return pos;
}