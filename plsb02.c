/* plsb02.c -- reads tokens, builds a logical line

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

static int initialized = FALSE;
static int is_open = FALSE;
static Sfile s;

/* We use the following as a pushback stack so that we can read  */
/* a token, push it onto the stack, and then read it again by    */
/* popping it back off.                                          */

/* The need to be able to deallocate anything left in this stack */
/* is what drives the open-read-close paradigm of this module.   */
/* The close gives us an opportunity to empty the stack.         */

/* Note that we can't read multiple Sfiles concurrently.  In     */
/* order to do that, the open would have to return some kind of  */
/* handle to a dynamically allocated object for retaining the    */
/* state of that input stream.  The client code would have to    */
/* pass the handle back when it wanted to get a logical line, in */
/* order to specify which input stream it wanted to get the line */
/* from.  All this is doable but is not likely to be useful.     */

static Toklist tokstack;

/* macros implement the stack operations: */

#define push_token(x) extend_toklist( &tokstack, (x) )
#define pop_token()   truncate_toklist( &tokstack )

static Pls_tok * next_token( void );
static Pls_tok * look_ahead( void );
static int begin_with_comment(
	const Pls_tok * pPrev, const Pls_tok * pComment );
static int sometimes_final( Pls_token_type type,
	const Toklist * TL, Pls_token_type next_type );
static int sometimes_first( Pls_token_type type,
	Pls_token_type next_type );

/*********************************************************************
 plsb_open -- prepare to read an Sfile to assemble logical lines.
 Make sure that we aren't already open.
 ********************************************************************/
int plsb_open( Sfile sfile )
{
	int rc = OKAY;

	ASSERT( FALSE == is_open );
	if( TRUE == is_open )
		return ERROR_FOUND;

	/* On the first call, initialize the token stack. */

	if( FALSE == initialized )
		rc = init_toklist( &tokstack, NULL );

	if( OKAY == rc )
	{
		/* Note that if we are open, we are also */
		/* initialized; the reverse is not true. */

		s = sfile;
		initialized = TRUE;
		is_open = TRUE;
	}

	return rc;
}

/*********************************************************************
 plsb_close -- shut down.  In particular, empty the token stack.  It
 is the client code's responsibility to close the Sfile.
 ********************************************************************/
void plsb_close( void )
{
	ASSERT( TRUE == is_open );
	if( TRUE == is_open )
	{
		empty_toklist( &tokstack );
		is_open = FALSE;
	}
}

/*********************************************************************
 get_logical_line -- read tokens and assemble them into a Toklist
 representing a logical line.
 ********************************************************************/
int get_logical_line( Toklist * pTL )
{
	int rc = OKAY;
	Pls_token_type type;
	Pls_tok * pT;

	/* sanity checks */

	ASSERT( s.p != NULL );
	ASSERT( pTL != NULL );

	rc = init_toklist( pTL, NULL );

	if( OKAY == rc )
	{
		int finished = FALSE;

		do
		{
			Probability finality;

			pT = next_token();
			if( NULL == pT )
			{
				rc = ERROR_FOUND;
				break;
			}
			else
			{
				type = pT->type;
				finality = is_final( type );
				rc = extend_toklist( pTL, pT );
				if( rc != OKAY )
					break;
			}

			/* At this point the new token is */
			/* the last one in the token list */

			if( P_always == finality )
				finished = TRUE;
			else
			{
				Pls_tok * pNext_token;

				pNext_token = look_ahead();
				if( NULL == pNext_token )
				{
					finished = TRUE;
					rc = ERROR_FOUND;
				}

				/* If the following remark is a comment,  */
				/* see if it should start on its own line */

				if( T_remark == pNext_token->type &&
					begin_with_comment( pT, pNext_token ) )
					finished = TRUE;
				else
				{
					Probability if_first;

					if_first = is_first( pNext_token->type );

					if( P_always == if_first )
						finished = TRUE;
					else if( P_usually == finality )
					{
						/* This is the end of the logical line, unless */
						/* the next token is eof or a comment. */

						if( T_eof    == pNext_token->type ||
							T_remark == pNext_token->type )
							finished = FALSE;
						else
							finished = TRUE;
					}
					else if( P_sometimes == finality )
						finished = sometimes_final( type, pTL,
							pNext_token->type );
					else if( P_sometimes == if_first )
						finished = sometimes_first( type,
							pNext_token->type );
				}
			}
		} while( FALSE == finished );
	}

	return rc;
}

/**********************************************************************
 next_token -- return a pointer to the next Pls_tok (ignoring white
 space).
 *********************************************************************/
static Pls_tok * next_token( void )
{
	Pls_tok * pT;

	/* First check the pushback stack.  This is a bit of a cheat; */
	/* we break encapsulation by accessing the innards of the     */
	/* Toklist directly.  It would be cleaner but less efficient  */
	/* to pop the stack first, and read the sFile if we get NULL. */

	/* Loop until we get NULL or something besides white space */

	for( ;; )
	{
		if( NULL == tokstack.pLast )
			pT = pls_next_tok( s );
		else
			pT = pop_token();

		if( NULL == pT )
			break;
		else if( T_whitespace == pT->type )
			pls_free_tok( &pT );	/* discard white space */
		else
			break;
	}

	return pT;
}

/**********************************************************************
 look_ahead -- return a token to the next Pls_tok, but leave it on the
 pushback stack.   This function is the main reason for implementing
 the pushback stack in the first place.
 *********************************************************************/
static Pls_tok * look_ahead( void )
{
	Pls_tok * pT;

	pT = next_token();
	if( pT != NULL )
		push_token( pT );

	return pT;
}

/**********************************************************************
 begin_with_comment -- determine whether a comment starts on a different
 line from the previous token.

 Note: we don't yet give the right answer if the previous token spans
 multiple lines.
 *********************************************************************/
static int begin_with_comment(
	const Pls_tok * pPrev, const Pls_tok * pComment )
{
	if( pPrev->line == pComment->line )
		return FALSE;
	else
		return TRUE;
}

/********************************************************************
 sometimes_final -- given the current token type and the next token
 type, return TRUE if the current token should be at the end of the
 logical line; otherwise return FALSE.
 *******************************************************************/
static int sometimes_final( Pls_token_type type,
	const Toklist * pTL, Pls_token_type next_type )
{
	switch( type )
	{
		case T_is :
			if( next_type == T_not  ||
				next_type == T_null )
				return FALSE;
			else
				return TRUE;
		case T_loop :
		{
			Pls_token_type first_type;

			ASSERT( pTL != NULL );
			ASSERT( pTL->pFirst != NULL );
			first_type = pTL->pFirst->type;
			if( T_for == first_type ||
				T_loop == first_type )
				return TRUE;
			else
				return FALSE;
		}
		case T_select :
			if( T_all == next_type ||
				T_distinct == next_type )
				return FALSE;
			else
				return TRUE;
		case T_union :
			if( T_all == next_type )
				return FALSE;
			else
            	return TRUE;
		default :
			return FALSE;
	}
}

/********************************************************************
 sometimes_first -- given the current token type and the next token
 type, return TRUE if the current token should be at the end of the
 logical line; otherwise return FALSE.
 *******************************************************************/
static int sometimes_first( Pls_token_type type,
	Pls_token_type next_type )
{
	switch( next_type )
	{
	case T_into :
		if( T_insert == type )
			return FALSE;
		else
			return TRUE;
	default :

		/* Not clear here; choice is arbitrary */

		return FALSE;
	}
}