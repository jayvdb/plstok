/* plsb05.c -- routines for parsing SELECT statements to add
   indentation.

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

static void do_select( Toknode * pTN );
static void do_select_list( Toknode * pTN );
static void do_into( Toknode * pTN );
static void do_into_list( Toknode * pTN );
static int  do_from( Toknode * pTN );
static int  do_from_list( Toknode * pTN );
static void do_where( Toknode * pTN );
static int  do_where_list( Toknode * pTN );
static void do_start( Toknode * pTN );
static int  do_start_clause( Toknode * pTN );
static void do_connect( Toknode * pTN );
static int  do_connect_clause( Toknode * pTN );
static void do_group( Toknode * pTN );
static void do_group_list( Toknode * pTN );
static void do_having( Toknode * pTN );
static int  do_having_list( Toknode * pTN );
static void do_splice( Toknode * pTN );
static void do_order( Toknode * pTN );
static void do_order_list( Toknode * pTN );
static void do_for( Toknode * pTN );
static void do_for_update( Toknode * pTN );
static void do_of( Toknode * pTN );
static void do_of_list( Toknode * pTN );
static void do_nowait( Toknode * pTN );

/*******************************************************************
 do_select_syntax -- traverse the logical line, parsing as you go,
 using a finite state machine.
 ******************************************************************/
int do_select_syntax( Toknode * pTN )
{
	int rc = OKAY;

	if( T_semicolon == pTN->type )
	{
		/* Found a semicolon -- the SQL statement is finished. */

		exit_all_levels();
	}
	else switch( curr_level.state )
	{
		case S_select :
			do_select( pTN );
			break;
		case S_select_list :
			do_select_list( pTN );
			break;
		case S_into :
			do_into( pTN );
			break;
		case S_into_list :
			do_into_list( pTN );
			break;
		case S_from :
			rc = do_from( pTN );
			break;
		case S_from_list :
			rc = do_from_list( pTN );
			break;
		case S_where :
			do_where( pTN );
			break;
		case S_where_list :
			rc = do_where_list( pTN );
			break;
		case S_start :
			do_start( pTN );
			break;
		case S_start_clause :
			rc = do_start_clause( pTN );
			break;
		case S_connect :
			do_connect( pTN );
			break;
		case S_connect_clause :
			rc = do_connect_clause( pTN );
			break;
		case S_group :
			do_group( pTN );
			break;
		case S_group_list :
			do_group_list( pTN );
			break;
		case S_having :
			do_having( pTN );
			break;
		case S_having_list :
			rc = do_having_list( pTN );
			break;
		case S_union :
		case S_intersect :
		case S_minus :
			do_splice( pTN );
			break;
		case S_order :
			do_order( pTN );
			break;
		case S_order_list :
			do_order_list( pTN );
			break;
		case S_for :
			do_for( pTN );
			break;
		case S_for_update :
			do_for_update( pTN );
			break;
		case S_of :
			do_of( pTN );
			break;
		case S_of_list :
			do_of_list( pTN );
			break;
		case S_nowait :
			do_nowait( pTN );
			break;
		default :
			break;
	}

	return rc;
}

/*******************************************************************
 do_select -- We have just found a SELECT.  Stay in this state until
 we find the first token other than DISTINCT, ALL, or a comment.
 ******************************************************************/
static void do_select( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_select :
		case T_remark :
		case T_distinct :
		case T_all :
			break;
		case T_lparens :
			++curr_level.parens_count;
			add_indent( pTN, S_select_list );
			break;
		default :
			add_indent( pTN, S_select_list );
			break;
	}
}

/*******************************************************************
 do_select_list -- We are amid the list of SELECTed items.  Stay in
 this state until we find INTO or FROM.
 ******************************************************************/
static void do_select_list( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_into :
			reduce_indent( pTN, S_into );
			break;
		case T_from :
			reduce_indent( pTN, S_from );
			break;
		case T_lparens :
			++curr_level.parens_count;
			break;
		case T_rparens :
			--curr_level.parens_count;
			break;
		case T_comma :
			/* We want each selected item on a separate line, so */
			/* we insert a line feed before the next token       */
			/* (provided it isn't a comment).  We are careful to */
			/* ignore a comma within parentheses. */

			if( 0 == curr_level.parens_count )
			{
				Toknode * pNext_node;

				pNext_node = pTN->pNext;
				if( pNext_node != NULL &&
					pNext_node->type != T_remark )
					pNext_node->lf = TRUE;
			}
			break;
		default :
			break;
	}
}

/*******************************************************************
 do_into -- We just found an INTO.  Stay in this state until we find
 a target.  (The only point of this state is to skip over any
 comments that may appear between INTO and the first identifier; no
 other tokens may legally appear here.)
 ******************************************************************/
static void do_into( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_identifier :
		case T_quoted_id :
			add_indent( pTN, S_into_list );
			break;
		default :
			break;
	}
}

/*******************************************************************
 do_into_list -- We are amid the list of INTO targets.  Stay in
 this state until we find FROM.
 ******************************************************************/
static void do_into_list( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_from :
			reduce_indent( pTN, S_from );
			break;
		case T_comma :
		{
			/* We want each target on a separate line, so  */
			/* we insert a line feed before the next token */
			/* (provided it isn't a comment). */

			Toknode * pNext_node;

			pNext_node = pTN->pNext;
			if( pNext_node != NULL &&
				pNext_node->type != T_remark )
				pNext_node->lf = TRUE;
			break;
		}
		default :
			break;
	}
}

/*******************************************************************
 do_from -- We just found a FROM.  Stay in this state until we find
 the beginning of a table name.
 ******************************************************************/
static int do_from( Toknode * pTN )
{
	int rc = OKAY;

	switch( pTN->type )
	{
		case T_identifier :
		case T_quoted_id :
			add_indent( pTN, S_from_list );
			break;
		case T_lparens :
			++curr_level.parens_count;
			break;
		case T_select :
			rc = push_level();
			if( OKAY == rc )
				add_indent( pTN, S_select );
			break;
		case T_rparens :
			if( curr_level.parens_count <= 0 )
			{
				pTN->lf = TRUE;
				pTN->indent_change = - curr_level.indents_count;
				pop_level();
			}
			--curr_level.parens_count;
			break;
		default :
			break;
	}
	return rc;
}

/*******************************************************************
 do_from_list -- We are amid the FROM list of tables.  Stay in
 this state until we find a semicolon (detected elsewhere) or any
 of WHERE, START, CONNECT, UNION, INTERSECT, MINUS, GROUP, ORDER, FOR,
 or SELECT.
 ******************************************************************/
static int do_from_list( Toknode * pTN )
{
	int rc = OKAY;

	switch( pTN->type )
	{
		case T_where :
			reduce_indent( pTN, S_where );
			break;
		case T_start :
			reduce_indent( pTN, S_start );
			break;
		case T_connect :
			reduce_indent( pTN, S_connect );
			break;
		case T_union :
			reduce_indent( pTN, S_union );
			break;
		case T_intersect :
			reduce_indent( pTN, S_intersect );
			break;
		case T_minus :
			reduce_indent( pTN, S_minus );
			break;
		case T_group :
			reduce_indent( pTN, S_group );
			break;
		case T_order :
			reduce_indent( pTN, S_order );
			break;
		case T_for :
			reduce_indent( pTN, S_for );
			break;
		case T_lparens :
			++curr_level.parens_count;
			break;
		case T_select :
			rc = push_level();
			if( OKAY == rc )
				add_indent( pTN, S_select );
			break;
		case T_rparens :
			if( curr_level.parens_count <= 0 )
			{
				pTN->lf = TRUE;
				pTN->indent_change = - curr_level.indents_count;
				pop_level();
			}
			--curr_level.parens_count;
			break;
		case T_comma :
			/* We want each table on a separate line, so we add */
			/* a line feed before the next token (provided it   */
			/* isn't a comment).  We are careful to ignore a    */
			/* comma within parentheses. */

			if( 0 == curr_level.parens_count )
			{
				Toknode * pNext_node;

				pNext_node = pTN->pNext;
				if( pNext_node != NULL &&
					pNext_node->type != T_remark )
					pNext_node->lf = TRUE;
			}
			break;
		default :
			break;
	}
	return rc;
}

/*******************************************************************
 do_where -- We just found a WHERE.  Stay in this state until we find
 something other than a comment.
 ******************************************************************/
static void do_where( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_remark :
			break;
		case T_lparens :
			++curr_level.parens_count;
			add_indent( pTN, S_where_list );
			break;
		default :
			add_indent( pTN, S_where_list );
			break;
	}
}

/*******************************************************************
 do_where_list -- We are amid the WHERE condition.  Stay in this
 state until we find a semicolon (detected elsewhere), START,
 CONNECT, UNION, INTERSECT, MINUS, GROUP, ORDER, FOR, or SELECT.
 ******************************************************************/
static int do_where_list( Toknode * pTN )
{
	int rc = OKAY;

	switch( pTN->type )
	{
		case T_start :
			reduce_indent( pTN, S_start );
			break;
		case T_connect :
			reduce_indent( pTN, S_connect );
			break;
		case T_union :
			reduce_indent( pTN, S_union );
			break;
		case T_intersect :
			reduce_indent( pTN, S_intersect );
			break;
		case T_minus :
			reduce_indent( pTN, S_minus );
			break;
		case T_group :
			reduce_indent( pTN, S_group );
			break;
		case T_order :
			reduce_indent( pTN, S_order );
			break;
		case T_for :
			reduce_indent( pTN, S_for );
			break;
		case T_lparens :
			++curr_level.parens_count;
			break;
		case T_select :
			rc = push_level();
			if( OKAY == rc )
				add_indent( pTN, S_select );
			break;
		case T_rparens :
			if( curr_level.parens_count <= 0 )
			{
				pTN->lf = TRUE;
				pTN->indent_change = - curr_level.indents_count;
				pop_level();
			}
			--curr_level.parens_count;
			break;
		default :
			break;
	}
	return rc;
}

/*******************************************************************
 do_start -- We just found a START.  Stay in this state until we find
 something other than WITH or a comment.
 ******************************************************************/
static void do_start( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_with :
		case T_remark :
			break;
		case T_lparens :
			++curr_level.parens_count;
			add_indent( pTN, S_start_clause );
		default :
			add_indent( pTN, S_start_clause );
			break;
	}
}

/*******************************************************************
 do_start_clause -- We are amid a START condition.  Stay in this
 state until we find a semicolon (detected elsewhere) or CONNECT.
 ******************************************************************/
static int do_start_clause( Toknode * pTN )
{
	int rc = OKAY;

	switch( pTN->type )
	{
		case T_connect :
			reduce_indent( pTN, S_connect );
			break;
		case T_lparens :
			++curr_level.parens_count;
			break;
		case T_rparens :
			--curr_level.parens_count;
			break;
		case T_select :
			rc = push_level();
			if( OKAY == rc )
				add_indent( pTN, S_select );
			break;
		default :
			break;
	}
	return rc;
}

/*******************************************************************
 do_connect -- We just found a CONNECT.  Stay in this state until we
 find something other than BY or a comment.
 ******************************************************************/
static void do_connect( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_by :
		case T_remark :
			break;
		case T_lparens :
			++curr_level.parens_count;
			add_indent( pTN, S_connect_clause );
		default :
			add_indent( pTN, S_connect_clause );
			break;
	}
}

/*******************************************************************
 do_connect_clause -- We are amid a CONNECT BY condition.  Stay in
 this state until we find a semicolon (detected elsewhere), START,
 CONNECT, UNION, INTERSECT, MINUS, GROUP, ORDER, FOR, or SELECT.
 ******************************************************************/
static int do_connect_clause( Toknode * pTN )
{
	int rc = OKAY;

	switch( pTN->type )
	{
		case T_start :
			reduce_indent( pTN, S_start );
			break;
		case T_connect :
			reduce_indent( pTN, S_connect );
			break;
		case T_union :
			reduce_indent( pTN, S_union );
			break;
		case T_intersect :
			reduce_indent( pTN, S_intersect );
			break;
		case T_minus :
			reduce_indent( pTN, S_minus );
			break;
		case T_group :
			reduce_indent( pTN, S_group );
			break;
		case T_order :
			reduce_indent( pTN, S_order );
			break;
		case T_for :
			reduce_indent( pTN, S_for );
			break;
		case T_lparens :
			++curr_level.parens_count;
			break;
		case T_select :
			rc = push_level();
			if( OKAY == rc )
				add_indent( pTN, S_select );
			break;
		case T_rparens :
			if( curr_level.parens_count <= 0 )
			{
				pTN->lf = TRUE;
				pTN->indent_change = - curr_level.indents_count;
				pop_level();
			}
			--curr_level.parens_count;
			break;
		default :
			break;
	}
	return rc;
}

/*******************************************************************
 do_group -- We just found a GROUP.  Stay in this state until we
 find something other than BY or a comment.
 ******************************************************************/
static void do_group( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_by :
		case T_remark :
			break;
		case T_lparens :
			++curr_level.parens_count;
			add_indent( pTN, S_group_list );
		default :
			add_indent( pTN, S_group_list );
			break;
	}
}

/*******************************************************************
 do_group_list -- We are amid the WHERE condition.  Stay in this
 state until we find a semicolon (detected elsewhere), START, CONNECT,
 UNION, INTERSECT, MINUS, GROUP, ORDER, FOR, or SELECT.
 ******************************************************************/
static void do_group_list( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_having :
			reduce_indent( pTN, S_having );
			break;
		case T_start :
			reduce_indent( pTN, S_start );
			break;
		case T_connect :
			reduce_indent( pTN, S_connect );
			break;
		case T_union :
			reduce_indent( pTN, S_union );
			break;
		case T_intersect :
			reduce_indent( pTN, S_intersect );
			break;
		case T_minus :
			reduce_indent( pTN, S_minus );
			break;
		case T_group :
			reduce_indent( pTN, S_group );
			break;
		case T_order :
			reduce_indent( pTN, S_order );
			break;
		case T_for :
			reduce_indent( pTN, S_for );
			break;
		case T_lparens :
			++curr_level.parens_count;
			break;
		case T_rparens :
			--curr_level.parens_count;
			break;
		case T_comma :
			/* We want each expression on a separate line, */
			/* so we add a line feed before the next token */
			/* (provided it isn't a comment).  We ignore a */
			/* comma within parentheses. */

			if( 0 == curr_level.parens_count )
			{
				Toknode * pNext_node;

				pNext_node = pTN->pNext;
				if( pNext_node != NULL &&
					pNext_node->type != T_remark )
					pNext_node->lf = TRUE;
			}
			break;
		default :
			break;
	}
}

/*******************************************************************
 do_having -- We just found a HAVING.  Stay in this state until we
 find something other than a comment.
 ******************************************************************/
static void do_having( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_remark :
			break;
		case T_lparens :
			++curr_level.parens_count;
			add_indent( pTN, S_having_list );
		default :
			add_indent( pTN, S_having_list );
			break;
	}
}

/*******************************************************************
 do_having_list -- We are amid a HAVING condition.  Stay in this
 state until we find a semicolon (detected elsewhere), START, CONNECT,
 UNION, INTERSECT, MINUS, GROUP, ORDER, FOR, or SELECT.
 ******************************************************************/
static int do_having_list( Toknode * pTN )
{
	int rc = OKAY;

	switch( pTN->type )
	{
		case T_start :
			reduce_indent( pTN, S_start );
			break;
		case T_connect :
			reduce_indent( pTN, S_connect );
			break;
		case T_union :
			reduce_indent( pTN, S_union );
			break;
		case T_intersect :
			reduce_indent( pTN, S_intersect );
			break;
		case T_minus :
			reduce_indent( pTN, S_minus );
			break;
		case T_group :
			reduce_indent( pTN, S_group );
			break;
		case T_order :
			reduce_indent( pTN, S_order );
			break;
		case T_for :
			reduce_indent( pTN, S_for );
			break;
		case T_lparens :
			++curr_level.parens_count;
			break;
		case T_select :
			rc = push_level();
			if( OKAY == rc )
				add_indent( pTN, S_select );
			break;
		case T_rparens :
			if( curr_level.parens_count <= 0 )
			{
				pTN->lf = TRUE;
				pTN->indent_change = - curr_level.indents_count;
				pop_level();
			}
			--curr_level.parens_count;
			break;
		default :
			break;
	}
	return rc;
}

/*******************************************************************
 do_splice -- We just found a UNION, INTERSECT, or MINUS, splicing
 the previous SELECT statement to another one.  We treat all of
 them the same.  Stay in this state until we find a SELECT.
 ******************************************************************/
static void do_splice( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_select :
			pTN->lf = TRUE;
			curr_level.state = S_select;
			break;
		default :
			break;
	}
}

/*******************************************************************
 do_order -- We just found an ORDER.  Stay in this state until we
 find something other than BY or a comment.
 ******************************************************************/
static void do_order( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_by:
		case T_remark :
			break;
		case T_lparens :
			++curr_level.parens_count;
			add_indent( pTN, S_order_list );
		default :
			add_indent( pTN, S_order_list );
			break;
	}
}

/*******************************************************************
 do_order_list -- We are amid the list of sort keys.  Stay in this
 state until we find a semicolon (detected elsewhere) or FOR.
 ******************************************************************/
static void do_order_list( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_for :
			reduce_indent( pTN, S_for );
			break;
		case T_lparens :
			++curr_level.parens_count;
			break;
		case T_rparens :
			if( curr_level.parens_count <= 0 )
			{
				pTN->lf = TRUE;
				pTN->indent_change = - curr_level.indents_count;
				pop_level();
			}
			--curr_level.parens_count;
			break;
		case T_comma :
		{
			/* We want each key on a separate line, so we */
			/* insert a line feed before the next token   */
			/* (provided it isn't a comment). */

			Toknode * pNext_node;

			pNext_node = pTN->pNext;
			if( pNext_node != NULL &&
				pNext_node->type != T_remark )
				pNext_node->lf = TRUE;
			break;
		}
		default :
			break;
	}
}

/*******************************************************************
 do_for -- We just found an ORDER.  Stay in this state until we
 find UPDATE.
 ******************************************************************/
static void do_for( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_update :
			curr_level.state = S_for_update;
		default :
			break;
	}
}

/*******************************************************************
 do_for_update -- We just found FOR UPDATE.  Stay in this state until
 we find OF, NOWAIT, ORDER, or FOR.
 ******************************************************************/
static void do_for_update( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_of:
			curr_level.state = S_of;
			break;
		case T_nowait :
			reduce_indent( pTN, S_nowait );
			break;
		case T_order :
			reduce_indent( pTN, S_order );
			break;
		case T_for :
			reduce_indent( pTN, S_for );
			break;
		case T_rparens :
			if( curr_level.parens_count <= 0 )
			{
				pTN->lf = TRUE;
				pTN->indent_change = - curr_level.indents_count;
				pop_level();
			}
			--curr_level.parens_count;
			break;
		default :
			break;
	}
}

/*******************************************************************
 do_of -- We just found OF after FOR UPDATE.  Stay in this
 state until we find something besides a comment.  (It should be
 an identifier or quoted identier, but we don't check for it.)
 ******************************************************************/
static void do_of( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_comment :
			break;
		default :
			add_indent( pTN, S_of_list );
			break;
	}
}

/*******************************************************************
 do_of_list -- We are amid a list of columns in a FOR UPDATE OF
 clause.  Stay in this state until we find a semicolon (detected
 elsewhere) or any of NOWAIT, ORDER, or FOR.
 ******************************************************************/
static void do_of_list( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_order :
			pTN->lf = TRUE;
			pTN->indent_change = -2;
			curr_level.indents_count -= 2;
			curr_level.state = S_order;
			break;
		case T_for :
			pTN->lf = TRUE;
			pTN->indent_change = -2;
			curr_level.indents_count -= 2;
			curr_level.state = S_for;
			break;
		case T_nowait :
			pTN->lf = TRUE;
			pTN->indent_change = -2;
			curr_level.indents_count -= 2;
			curr_level.state = S_nowait;
			break;
		case T_comma :
		{
			/* We want each column on a separate line, so  */
			/* we insert a line feed before the next token */
			/* (provided it isn't a comment). */

			Toknode * pNext_node;

			pNext_node = pTN->pNext;
			if( pNext_node != NULL &&
				pNext_node->type != T_remark )
				pNext_node->lf = TRUE;
			break;
		}
		case T_rparens :
			if( curr_level.parens_count <= 0 )
			{
				pTN->lf = TRUE;
				pTN->indent_change = - curr_level.indents_count;
				pop_level();
			}
			--curr_level.parens_count;
			break;
		default :
			break;
	}
}

/*******************************************************************
 do_nowait -- We just found a NOWAIT.  Stay in this state until we
 find a semicolon (detected elsewhere), ORDER, or FOR.
 ******************************************************************/
static void do_nowait( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_order :
			curr_level.state = S_order;
			break;
		case T_for :
			curr_level.state = S_for;
			break;
		case T_rparens :
			if( curr_level.parens_count <= 0 )
			{
				pTN->lf = TRUE;
				pTN->indent_change = - curr_level.indents_count;
				pop_level();
			}
			--curr_level.parens_count;
			break;
		default :
			break;
	}
}
