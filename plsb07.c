/* plsb07.c -- routines for parsing UPDATE statements to add
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

static int  do_update( Toknode * pTN );
static void do_subquery( Toknode * pTN );
static int  do_set( Toknode * pTN );
static int  do_set_list( Toknode * pTN );
static int  do_set_subquery( Toknode * pTN );
static int  do_set_comma( Toknode * pTN );
static void do_where( Toknode * pTN );
static int  do_where_list( Toknode * pTN );

/*******************************************************************
 do_update_syntax -- traverse the logical line, parsing as you go,
 using a finite state machine.
 ******************************************************************/
int do_update_syntax( Toknode * pTN )
{
	int rc = OKAY;

	if( T_semicolon == pTN->type )
	{
		/* Found a semicolon -- the SQL statement is finished. */

		exit_all_levels();
	}
	else switch( curr_level.state )
	{
		case S_update :
			rc = do_update( pTN );
			break;
		case S_subquery :
			do_subquery( pTN );
			break;
		case S_set :
			rc = do_set( pTN );
			break;
		case S_set_list :
			rc = do_set_list( pTN );
			break;
		case S_set_subquery :
			rc = do_set_subquery( pTN );
			break;
		case S_set_comma :
			rc = do_set_comma( pTN );
			break;
		case S_where :
			do_where( pTN );
			break;
		case S_where_list :
			rc = do_where_list( pTN );
			break;
		default :
			break;
	}

	return rc;
}

/*******************************************************************
 do_update -- We have just found UPDATE.  Look for a left parens
 (signifying the beginning of a subquery) or SET.
 ******************************************************************/
static int do_update( Toknode * pTN )
{
	int rc = OKAY;

	switch( pTN->type )
	{
		case T_set :
			pTN->lf = TRUE;
			curr_level.state = S_set;
			break;
		case T_lparens :
			++curr_level.parens_count;
			add_indent( pTN, S_subquery );
			rc = push_level();
			if( OKAY == rc )
			{
				curr_level.s_type = ST_select;
				curr_level.state  = S_select;
			}
			break;
		default :
			break;
	}
	return rc;
}

/*******************************************************************
 do_subquery -- We just finished a subquery after UPDATE.  Look for
 SET.
 ******************************************************************/
static void do_subquery( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_set :
			reduce_indent( pTN, S_set );
			break;
		case T_rparens :
			--curr_level.parens_count;
			break;
		case T_lparens :
			++curr_level.parens_count;
			break;
		case T_comma :
			if( 0 == curr_level.parens_count &&
				pTN->pNext != NULL &&
				pTN->pNext->type != T_remark )
				pTN->pNext->lf = TRUE;
			break;
		default :
			break;
	}
}

/*******************************************************************
 do_set -- We just found SET.  The next thing we see should (other
 than a comment) should be a left parenthesis or an identifier.
 Put in on the next line with an indent.
 ******************************************************************/
static int do_set( Toknode * pTN )
{
	int rc = OKAY;

	switch( pTN->type )
	{
		case T_identifier :
			add_indent( pTN, S_set_list );
			break;
		case T_lparens :
			++curr_level.parens_count;
			add_indent( pTN, S_set_list );
			break;
		default :
			break;
	}
	return rc;
}

/*******************************************************************
 do_set_list -- We are amid the assignments in a SET clause.
 ******************************************************************/
static int do_set_list( Toknode * pTN )
{
	int rc = OKAY;

	switch( pTN->type )
	{
		case T_where :
			reduce_indent( pTN, S_where );
			break;
		case T_rparens :
			--curr_level.parens_count;
			break;
		case T_lparens :
			++curr_level.parens_count;
			break;
		case T_comma :
			if( 0 == curr_level.parens_count &&
				pTN->pNext != NULL &&
				pTN->pNext->type != T_remark )
				pTN->pNext->lf = TRUE;
			break;
		case T_select :
			add_indent( pTN, S_set_subquery );
			rc = push_level();
			if( OKAY == rc )
			{
				curr_level.s_type = ST_select;
				curr_level.state  = S_select;
			}
			break;
		default :
			break;
	}
	return rc;
}

/*******************************************************************
 do_set_subquery -- We just left a subquery in a SET clause.  Look
 for WHERE or a comma.  Nothing else is valid SQL here (except a
 comment).
 ******************************************************************/
static int do_set_subquery( Toknode * pTN )
{
	int rc = OKAY;

	switch( pTN->type )
	{
		case T_where :
			reduce_indent( pTN, S_where );
			if( curr_level.indents_count > 0 )
				--curr_level.indents_count;
			pTN->indent_change = -2;
			break;
		case T_comma :
			if( pTN->pNext != NULL &&
				pTN->pNext->type != T_remark )
				pTN->pNext->lf = TRUE;
			curr_level.state = S_set_comma;
			break;
		default :
			break;
	}
	return rc;
}

/*******************************************************************
 do_set_comma -- We just found a comma after a subquery in a SET
 clause.  All we should find (other than a comment) is an
 identifier or a left parens (signifying a list of columns).
 ******************************************************************/
static int do_set_comma( Toknode * pTN )
{
	int rc = OKAY;

	switch( pTN->type )
	{
		case T_lparens :
			++curr_level.parens_count;
			reduce_indent( pTN, S_set_list );
			break;
		case T_identifier :
			reduce_indent( pTN, S_set_list );
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
 state until we find a semicolon (detected elsewhere) or SELECT.
 ******************************************************************/
static int do_where_list( Toknode * pTN )
{
	int rc = OKAY;

	switch( pTN->type )
	{
		case T_lparens :
			++curr_level.parens_count;
			break;
		case T_select :
			rc = push_level();
			if( OKAY == rc )
			{
				add_indent( pTN, S_where_list );
				curr_level.s_type = ST_select;
				curr_level.state  = S_select;
			}
			break;
		case T_rparens :
			--curr_level.parens_count;
			break;
		default :
			break;
	}
	return rc;
}



