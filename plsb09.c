/* plsb09.c -- routines for parsing CURSOR and FETCH statements to add
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

void do_fetch( Toknode * pTN );
static void do_into( Toknode * pTN );
static void do_into_list( Toknode * pTN );

/*******************************************************************
 do_cursor_syntax -- look for SELECT, then switch to the machine for
 indenting SELECT statements.
 ******************************************************************/
int do_cursor_syntax( Toknode * pTN )
{
	int rc = OKAY;

	if( T_select == pTN->type )
	{
		rc = push_level();
		if( OKAY == rc )
		{
			curr_level.s_type = ST_select;
			add_indent( pTN, S_select );
		}
	}
	else if( T_semicolon == pTN->type )
	{
		/* Found a semicolon -- the CURSOR statement is finished. */

		exit_all_levels();
	}

	return rc;
}

/*******************************************************************
 do_fetch_syntax -- traverse the logical line, parsing as you go,
 using a finite state machine.
 ******************************************************************/
int do_fetch_syntax( Toknode * pTN )
{
	int rc = OKAY;

	if( T_semicolon == pTN->type )
	{
		/* Found a semicolon -- the SQL statement is finished. */

		exit_all_levels();
	}
	else switch( curr_level.state )
	{
		case S_fetch :
			do_fetch( pTN );
			break;
		case S_into :
			do_into( pTN );
			break;
		case S_into_list :
			do_into_list( pTN );
			break;
		default :
			break;
	}

	return rc;
}

/*******************************************************************
 do_fetch -- We have just found FETCH.  Look for INTO.
 ******************************************************************/
static void do_fetch( Toknode * pTN )
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
 do_into -- We have just found an INTO after FETCH.  Look for
 an identifier.
 ******************************************************************/
static void do_into( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_identifier :
			add_indent( pTN, S_into_list );
			break;
		default :
			break;
	}
}

/*******************************************************************
 do_into_list -- We are specifying the variables into which we
 are fetching.  Put each one on a separate line.
 ******************************************************************/
static void do_into_list( Toknode * pTN )
{
	switch( pTN->type )
	{
		case T_comma :
		{
			/* We want each variable on a separate line, so we */
			/* add a line feed before the next token (provided */
			/* it isn't a comment). */

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



