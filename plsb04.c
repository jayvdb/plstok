/* plsb04.c -- routines for parsing particular kinds of syntax which
   don't respond well to the simplistic approach of looking at the
   first token of each logical line.  In particular: SQL syntax, though
   other special cases could be introduced here as well.

   Depending on the current context, we branch to a finite state
   machine specialized for that context.  The machine traverses the
   logical line, interprets the syntax (still rather crudely),
   annotates the Toknodes for additional line feeds and indentation,
   and eventually recognizes the end of its context.

   In order to handle a subquery (a SELECT context which may be
   imbedded within some other kind of context) we maintain a stack
   of nested levels (not yet implemented).

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

Syntax_level curr_level =
{
	ST_none,
	S_none,
	0,
	0,
	NULL
};

static Syntax_level * level_stack = NULL;
static Syntax_level * freelist    = NULL;

static void scavenge_levels( void * dummy );

/********************************************************************
 edit_syntax -- Examine the syntax (rather crudely) of the logical
 line and decide where to put additional line feeds and indentation.
 Annotate the Toknodes accordingly.

 This layer of analysis is intended mainly, if not exclusively,
 for SQL statements.  Since there is no END SELECT statement, the
 approach used for the procedural code doesn't work very well,
 because there's no good way to tell when to unindent.
 *******************************************************************/
int edit_syntax( Toklist * pTL )
{
	int rc = OKAY;
	Toknode * pTN;

	/* For now at least, the signal to enter a parsing context */
	/* will always be the first token in a logical line. The   */
	/* following logic will do for now, but it will have to    */
	/* become fancier in order to handle subqueries. */

	/* There is an obscure possibility for confusion here.  */
	/* The keyword DELETE can refer, not to a database      */
	/* operation, but to a PL/SQL table (i.e. an array-like */
	/* object in memory.  It is possible, though perverse,  */
	/* for such a DELETE to arrive at the beginning of a    */
	/* logical line due to a preceding comment.   The       */
	/* consequences, if any, will probably be minor.        */

	/* Similar confusion could arise if, for example, a     */
	/* package uses SELECT as a procedure or function name. */

	if( S_none == curr_level.state )
	{
		switch( pTL->pFirst->type )
		{
			case T_select :
				rc = push_level();
				curr_level.s_type = ST_select;
				curr_level.state  = S_select;
				break;
			case T_insert :
				curr_level.s_type = ST_insert;
				curr_level.state  = S_insert;
				break;
			case T_update :
				curr_level.s_type = ST_update;
				curr_level.state  = S_update;
				break;
			case T_delete :
				curr_level.s_type = ST_delete;
				break;
			case T_cursor :
				curr_level.s_type = ST_cursor;
				curr_level.state  = S_cursor;
				break;
			case T_fetch :
				curr_level.s_type = ST_fetch;
				curr_level.state  = S_fetch;
				break;
			default :
				break;
		}
	}

	/* Dispatch each token in the logical line to a state     */
	/* machine according to what kind of syntax we're editing */
	/* (not all are implemented yet) */

	if( OKAY == rc )
	{
		for( pTN = pTL->pFirst; pTN != NULL; pTN = pTN->pNext )
		{
			switch( curr_level.s_type )
			{
			case ST_none :
				break;
			case ST_select :
				rc = do_select_syntax( pTN );
				break;
			case ST_insert :
				rc = do_insert_syntax( pTN );
				break;
			case ST_update :
				rc = do_update_syntax( pTN );
				break;
			case ST_delete :
				break;
			case ST_cursor :
				rc = do_cursor_syntax( pTN );
				break;
			case ST_fetch :
				rc = do_fetch_syntax( pTN );
				break;
			default  :
				break;		/* should be unreachable */
			}
			if( rc != OKAY )	/* abort loop on error */
				break;
		}
	}
	return rc;
}

/*******************************************************************
 exit_all_levels -- We have reached the end of a special parsing
 zone.  Empty the stack of levels.  If the levels contain any
 outstanding indentation, issue a request to cancel it at the
 beginning of the next logical line.
 ******************************************************************/
void exit_all_levels( void )
{
	int total_indents;

	total_indents = curr_level.indents_count;
	while( level_stack != NULL )
	{
		pop_level();
		total_indents += curr_level.indents_count;
	}

	curr_level.indents_count = 0;
	curr_level.s_type = ST_none;
	curr_level.state  = S_none;

	defer_unindent( total_indents );
}

/*******************************************************************
 pop_level -- pop the top of the level stack, overwriting
 curr_level.  Deallocate the popped level by sticking it on the
 free list.
 ******************************************************************/
void pop_level( void )
{
	if( level_stack != NULL )
	{
		Syntax_level * pOld;

		/* Remove the level, copy it to curr_level */

		pOld = level_stack;
		level_stack = level_stack->pNext;
		curr_level = *pOld;
		curr_level.pNext = NULL;	/* a gesture for good hygiene */

		/* Deallocate it */

		pOld->pNext = freelist;
		freelist = pOld;
	}
}
/*******************************************************************
 push_level -- allocate and initialize a Syntax_level (copying
 from curr_level); save it on the stack.  Return OKAY if successful
 or ERROR_FOUND if not.
 ******************************************************************/
int push_level( void )
{
	Syntax_level * new_level;

	/* Allocate from the free_list if */
	/* possible, else from the heap.  */

	if( NULL == freelist )
	{
		static int first_time = TRUE;

		new_level = allocMemory( sizeof( Syntax_level ) );
		if( NULL == new_level )
			return ERROR_FOUND;

		if( TRUE == first_time )
		{
			/* First time? install a memory scavenger */

			registerMemoryPool( scavenge_levels, NULL );
			first_time = FALSE;
		}
	}
	else
	{
		new_level = freelist;
		freelist  = freelist->pNext;
	}

	ASSERT( new_level != NULL );

	/* Push the level onto the stack */

	*new_level = curr_level;
	new_level->pNext = level_stack;
	level_stack = new_level;

	curr_level.parens_count = 0;
	curr_level.indents_count = 0;

	return OKAY;
}

/********************************************************************
 scavenge_levels -- a memory scavenger to clear out the free list.
 *******************************************************************/
static void scavenge_levels( void * dummy )
{
	Syntax_level * pSL;
	Syntax_level * pTemp;

	for( pSL = freelist; pSL != NULL; )
	{
		pTemp = pSL->pNext;
		freeMemory( pSL );
		pSL = pTemp;
	}
}

/********************************************************************
 add_indent -- annotate a token to begin a new level of indentation
 and change state.  This function, like the next one, is not a
 well-encapsulated abstraction, just a convenient gimmick for reducing
 the sheer bulk of the code.
 ********************************************************************/
void add_indent( Toknode * pTN, S_state state )
{
	ASSERT( pTN != NULL );
	if( pTN != NULL )
	{
		pTN->lf = TRUE;
		pTN->indent_change = 1;
		++curr_level.indents_count;
		curr_level.state = state;
	}
}

/*******************************************************************
 reduce_indent -- annotate a token to unindent and change state.
 ******************************************************************/
void reduce_indent( Toknode * pTN, S_state state )
{
	ASSERT( pTN != NULL );
	if( pTN != NULL )
	{
		pTN->lf = TRUE;
		pTN->indent_change = -1;
		--curr_level.indents_count;
		curr_level.state = state;
	}
}


