/* plsb00.c -- a home for whatever things should, or might, be
   configurable.

   At this point nothing is configurable.  Everything is hard-coded.

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

Pls_globals pls_globals;

/*******************************************************************
 is_final -- report the likelihood that a given token type is the
 last token on a logical line.
 *******************************************************************/
Probability is_final( Pls_token_type type )
{
	switch ( type )
	{
	case T_remark :
	case T_eof :
		return P_always;
	case T_semicolon :
	case T_then :
	case T_else :
	case T_begin :
	case T_exception :
	case T_from :
	case T_where :
	case T_distinct :
	case T_declare :
	case T_minus :
	case T_intersect :
		return P_usually;
	case T_is :
	case T_select :
	case T_union :
		return P_sometimes;
	default :
		return P_sometimes;
	}
}

/*****************************************************************
 is_first -- report the likelihood that a given type of token
 should fall at the beginning of a line.
 ****************************************************************/
Probability is_first( Pls_token_type type )
{
	switch( type )
	{
		case T_select :
		case T_from :
		case T_where :
		case T_order :
		case T_for :
		case T_values :
		case T_set :
		case T_union :
		case T_minus :
		case T_intersect :
			return P_always;
		case T_into :
			return P_sometimes;
		default :
			return P_never;
	}
}

/*****************************************************************
 if_space -- decide whether there should be a blank space between
 two adjacent tokens.  Return TRUE or FALSE.
 ****************************************************************/
int need_space( Pls_token_type first, Pls_token_type second )
{
	/* some token types are always preceded by a blank space, */
	/* and others never are. */

	switch( second )
	{
		case T_semicolon :
		case T_eof :
		case T_percent :
		case T_comma :
		case T_dot :
		case T_at_sign :
		case T_range_dots :
		case T_right_label :
			return FALSE;
		case T_quoted_id :
		case T_string_lit :
		case T_char_lit :
		case T_num_lit :
		case T_remark :
		case T_plus :
		case T_minus_sign :
		case T_star :
		case T_virgule :
		case T_equals :
		case T_less :
		case T_greater :
		case T_rparens :
		case T_colon :
		case T_expo :
		case T_not_equal :
		case T_tilde :
		case T_hat :
		case T_less_equal :
		case T_greater_equal :
		case T_assignment :
		case T_arrow :
		case T_bars :
		case T_left_label :
			return TRUE;
		case T_lparens :
			if( T_identifier == first ||
				T_varchar2   == first ||
				T_number     == first ||
				T_char       == first )
				return FALSE;
			else
				return TRUE;
		default :
			break;
	}

	switch( first )
	{
	case T_dot :
	case T_percent :
	case T_range_dots :
		return FALSE;
	default :
		return TRUE;
	}
}

/*******************************************************************
 need_indent: Decide whether to indent after a logical line
 beginning with a token of a specified type.
 ******************************************************************/
Probability need_indent( Pls_token_type type )
{
	switch( type )
	{
		case T_if :
		case T_else :
		case T_elsif :
		case T_when :
		case T_loop :
		case T_while :
		case T_begin :
		case T_exception :
		case T_into :
			return P_always;
		case T_for :
			return P_sometimes;
		default :
			return P_never;
	}
}

/********************************************************************
 need_unindent -- given the first token type for a logical line,
 decide whether to unindent.
 *******************************************************************/
int need_unindent( Pls_token_type type )
{
	switch( type )
	{
		case T_end :
		case T_else :
		case T_elsif :
		case T_exception :
		case T_when :
		case T_into :
/*		case T_from :	*/
			return TRUE;
		default :
			return FALSE;
	}
}

/************************************************************************
 plsb_init -- initialize whatever needs to be initialized, in particular
 pls_globals.  Ultimately this will be a home for code to read a
 configuration file.
 ***********************************************************************/
int plsb_init( void )
{
	pls_globals.indentation = 0;
	pls_globals.indent_string = "    ";
	pls_globals.soft_indent_string = "  ";
	return OKAY;
}
