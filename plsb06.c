/* plsb06.c -- routines for parsing INSERT statements to add
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

static void do_insert( Toknode * pTN );
static int  do_into( Toknode * pTN );
static int  do_subquery( Toknode * pTN );
static int  do_into_list( Toknode * pTN );
static void do_column_list_a( Toknode * pTN );
static void do_column_list_b( Toknode * pTN );
static int  do_column_list_c( Toknode * pTN );
static void do_values( Toknode * pTN );
static void do_values_list_a( Toknode * pTN );
static void do_values_list_b( Toknode * pTN );

/*******************************************************************
 do_insert_syntax -- traverse the logical line, parsing as you go,
 using a finite state machine.
 ******************************************************************/
int do_insert_syntax( Toknode * pTN )
{
	int rc = OKAY;

	if( T_semicolon == pTN->type )
	{
		/* Found a semicolon -- the SQL statement is finished. */

		exit_all_levels();
	}
	else switch( curr_level.state )
	{
		case S_insert :
			do_insert( pTN );
			break;
		case S_into :
			rc = do_into( pTN );
			break;
		case S_into_list :
			rc = do_into_list( pTN );
			break;
		case S_subquery :
			rc = do_subquery( pTN );
			break;
		case S_column_list_a :
			do_column_list_a( pTN );
			break;
		case S_column_list_b :
			do_column_list_b( pTN );
			break;
		case S_column_list_c :
			rc = do_column_list_c( pTN );
			break;
		case S_values :
			do_values( pTN );
			break;
		case S_values_list_a :
			do_values_list_a( pTN );
			break;
		case S_values_list_b :
			do_values_list_b( pTN );
			break;
		default :
			/* The default case includes S_values_list_c, which  */
			/* occurs after the end of the values list.  At this */
			/* point no token is valid other than comments and a */
			/* semicolon.  The former we ignore, and the latter  */
			/* we handle above.  So there is nothing to do in    */
			/* this state. */
				break;
	}

	return rc;
}

/*******************************************************************
 do_insert -- We have just found INSERT.  Look for INTO.
 ******************************************************************/
static void do_insert( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_into :
			curr_level.state = S_into;
			break;
		default :
			break;
	}
}

/*******************************************************************
 do_into -- We have just found an INTO after INSERT.  Look for
 an identifier (starting a table or view name) or a left parens
 (starting a subquery).
 ******************************************************************/
static int do_into( Toknode * pTN )
{
	int rc = OKAY;

	switch( pTN->type )
	{
		case T_identifier :
			curr_level.state = S_into_list;
			break;
		case T_lparens :
			++curr_level.parens_count;
			add_indent( pTN, S_subquery );
			rc = push_level();
			if( OKAY == rc )
			{
				curr_level.s_type = ST_select;
				curr_level.state  = S_select;
				pTN->lf = TRUE;
			}
			break;
		default :
			break;
	}
	return rc;
}

/*******************************************************************
 do_subquery -- We just finished a subquery after INSERT INTO.  Look
 for VALUES, a column list, or SELECT (starting another subquery).
 ******************************************************************/
static int do_subquery( Toknode * pTN )
{
	int rc = OKAY;

	switch( pTN->type )
	{
		case T_values :
			reduce_indent( pTN, S_values );
			break;
		case T_lparens :
			++curr_level.parens_count;
			reduce_indent( pTN, S_column_list_a );
			break;
		case T_select :
			rc  = push_level();
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
 do_into_list -- We are specifying the table into which we are
 inserting.  Look for a left parenthesis (signifying the beginning
 of a column list), VALUES, or SELECT (signifying a subquery).
 ******************************************************************/
static int do_into_list( Toknode * pTN )
{
	int rc = OKAY;

	switch( pTN->type )
	{
		case T_values :
			curr_level.state = S_values;
			break;
		case T_lparens :
			++curr_level.parens_count;
			curr_level.state = S_column_list_a;
			pTN->lf = TRUE;
			break;
		case T_select :
			rc = push_level();
			if( OKAY == rc )
			{
				curr_level.s_type = ST_select;
				add_indent( pTN, S_select );
			}
			break;
		default :
			break;
	}
	return rc;
}

/*******************************************************************
 do_column_list_a -- We just started a list of columns into which we
 are inserting.  Look for the first column name.
 ******************************************************************/
static void do_column_list_a( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_identifier :
			add_indent( pTN, S_column_list_b );
			break;
		case T_lparens :
			++curr_level.parens_count;
			break;
		case T_rparens :
			if( curr_level.parens_count > 0 )
				--curr_level.parens_count;
			break;
		default :
			break;
	}
}

/*******************************************************************
 do_column_list_b -- We are amid the list of columns into which we
 are inserting.  Look for a closing right parenthesis.
 ******************************************************************/
static void do_column_list_b( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_lparens :
			++curr_level.parens_count;
			break;
		case T_rparens :
			if( curr_level.parens_count > 0 )
			{
				--curr_level.parens_count;
				if( curr_level.parens_count < 1 )
					reduce_indent( pTN, S_column_list_c );
			}
			break;
		case T_comma :
			/* We want each column on a separate line, so we add */
			/* a line feed before the next token (provided it    */
			/* isn't a comment).  We are careful to ignore a     */
			/* comma within nested parentheses. */

			if( 1 == curr_level.parens_count )
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
 do_column_list_c -- We just finished a list of columns into which we
 are inserting.  Look for VALUES or SELECT.
 ******************************************************************/
static int do_column_list_c( Toknode * pTN )
{
	int rc = OKAY;

	switch( pTN->type )
	{
		case T_values :
			curr_level.state = S_values;
			break;
		case T_select :
			rc = push_level();
			if( OKAY == rc )
			{
				curr_level.s_type = ST_select;
				add_indent( pTN, S_select );
			}
			break;
		default :
			break;
	}
	return rc;
}

/*******************************************************************
 do_values -- We just found VALUES.  Look for the parenthesis which
 starts the list of values.
 ******************************************************************/
static void do_values( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_lparens :
			++curr_level.parens_count;
			curr_level.state = S_values_list_a;
			pTN->lf = TRUE;
			break;
		default :
			break;
	}
}

/*******************************************************************
 do_values_list_a -- We just found the parenthesis which starts the
 list of values.  Look for the first expression in the values list
 (i.e. just about anything but a comment).
 ******************************************************************/
static void do_values_list_a( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_remark :
			break;
		case T_lparens :
			++curr_level.parens_count;
			add_indent( pTN, S_values_list_b );
			break;
		case T_rparens :	/* shouldn't happen */
			if( curr_level.parens_count > 0 )
			{
				--curr_level.parens_count;
				if( curr_level.parens_count < 1 )
					reduce_indent( pTN, S_values_list_c );
			}
			break;
		default :
			add_indent( pTN, S_values_list_b );
			break;
	}
}

/*******************************************************************
 do_values_list_b -- We are amid the list of values to be inserted.
 Look for the closing right parenthesis.
  ******************************************************************/
static void do_values_list_b( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_lparens :
			++curr_level.parens_count;
			break;
		case T_rparens :
			if( curr_level.parens_count > 0 )
			{
				--curr_level.parens_count;
				if( curr_level.parens_count < 1 )
					reduce_indent( pTN, S_values_list_c );
			}
			break;
		case T_comma :
			/* We want each expression on a separate line, so we */
			/* add a line feed before the next token (provided   */
			/* it isn't a comment).  We are careful to ignore a  */
			/* comma within nested parentheses. */

			if( 1 == curr_level.parens_count )
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
