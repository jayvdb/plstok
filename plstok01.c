/* plstok01.c -- routines for tokenizing PL/SQL

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
#include <ctype.h>
#include "util.h"
#include "sfile.h"
#include "plstok.h"
#include "plspriv.h"

#define LOCAL_BUFLEN 63

static int preserving = TRUE;

static int get_word( Pls_tok * pT, Sfile s, int c );
static int get_squote( Pls_tok * pT, Sfile s );
static int get_punct( Pls_tok * pT, Sfile s, int c );
static int get_whitespace( Pls_tok * pT, Sfile s, int c );
static int get_c_comment( Pls_tok * pT, Sfile s );
static int get_dquote( Pls_tok * pT, Sfile s );
static int get_hyphen_comment( Pls_tok * pT, Sfile s );
static int get_number( Pls_tok * pT, Sfile s );

/******************************************************************
 pls_preserve -- set a switch denoting that we shall preserve
 white space and comments.  Return: prior value of the switch.
 *****************************************************************/
int pls_preserve( void )
{
	int prior_value;

	prior_value = preserving;
	preserving  = TRUE;
	return prior_value;
}

/******************************************************************
 pls_nopreserve -- set a switch denoting that we shall discard
 white space and comments.  Return: prior value of the switch.
 *****************************************************************/
int pls_nopreserve( void )
{
	int prior_value;

	prior_value = preserving;
	preserving  = FALSE;
	return prior_value;
}

/******************************************************************
 pls_next_tok -- Allocate a token and return a pointer to it.  It
 is the client code's responsibility to free it by calling
 pls_free_tok().

 Errors are indicated by returning an error token.  The only
 exception is an inability to allocate memory for a token, in
 which case we return NULL.
 *****************************************************************/
Pls_tok * pls_next_tok( Sfile s )
{
	int rc;
	Pls_tok * pT = NULL;
	int c;
	Sposition pos;

	pT = pls_alloc_tok();
	if( NULL == pT )
		return NULL;

	/* In the following loop, the funky while clause is       */
	/* designed to skip over comments if preserving is FALSE. */

	do
	{
		c = s_getc( s );

		if( FALSE == preserving )
		{
			/* Discard all white space. */

			while( isspace( (unsigned char) c ) )
				c = s_getc( s );
		}

		pos = s_position( s );
		pT->line = pos.line;
		pT->col  = pos.col;

		if( EOF == c )
		{
			pT->type = T_eof;
			rc = OKAY;
		}
		else if( isspace( (unsigned char) c ) )
			rc = get_whitespace( pT, s, c );
		else if( isalpha( (unsigned char) c ) )
			rc = get_word( pT, s, c );
		else if( ispunct( (unsigned char) c ) )
			rc = get_punct( pT, s, c );
		else if( isdigit( (unsigned char) c ) )
		{
			(void) s_ungetc( s, c );
			rc = get_number( pT, s );
		}
		else
			rc = pls_append_msg( pT, "Unexpected character" );
	} while( FALSE == preserving && T_remark == pT->type );

	if( rc != OKAY )
		pls_free_tok( &pT );

	return pT;
}

/*****************************************************************
 get_word -- collect characters into a word.  It may turn out to
 be either a reserved word or an identifier.
 ****************************************************************/
static int get_word( Pls_tok * pT, Sfile s, int c )
{
	int count = 1;             /* How many characters in buffer */
	int total_count = 0;       /* Total characters collected */
	int finished = FALSE;
	char buf[ LOCAL_BUFLEN + 1 ];

	ASSERT( pT != NULL );

	buf[ 0 ] = c;

	while( FALSE == finished )
	{
		/* Collect all the eligible characters  */

		c = s_getc( s );

		if( isalnum( (unsigned char) c ) ||
			'_' == c || '$' == c || '#' == c )
		{
			if( count < LOCAL_BUFLEN )
				buf[ count++ ] = c;
			else
			{
				/* flush the buffer and start over */

				total_count += count;
				buf[ count ] = '\0';
				if( pls_append_text( pT, buf ) != OKAY )
					return ERROR_FOUND;
				else
				{
					buf[ 0 ] = c;
					count = 1;
				}
			}
		}
		else
		{
			/* Since we have read one character past the word, we */
			/* put the last character back into the input stream  */

			(void) s_ungetc( s, c );
			finished = TRUE;
		}
	}

	if( count > 0 )
	{
		/* flush the last buffer */

		total_count += count;
		buf[ count ] = '\0';
		if( pls_append_text( pT, buf ) != OKAY )
			return ERROR_FOUND;
	}

	if( total_count > PLS_MAX_WORD - 2 ) /* subtract 2: not quoted */
	{
		pT->type = T_error;
		return pls_append_msg( pT, "Identifier is too long" );
	}
	else
	{
		pT->type = pls_keyword( pT->buf );
		return OKAY;
	}
}

/*****************************************************************
 get_squote -- collect characters within single quotes, regarding
 a consecutive pair of single quotes as text rather than delimiters.

 Note that we collect the delimiting quote characters as well as
 the text inside.
 ****************************************************************/
static int get_squote( Pls_tok * pT, Sfile s )
{
	int rc = OKAY;
	int c;
	int count = 1;             /* How many characters in buffer */
	int total_count = 0;       /* Total characters collected */
	int after_quote = FALSE;
	int finished = FALSE;
	char buf[ LOCAL_BUFLEN + 1 ];

	ASSERT( pT != NULL );

	buf[ 0 ] = '\'';

	do
	{
		/* Collect each eligible character */

		c = s_getc( s );

		/* Determine whether we've reached the end.  The last eligible */
		/* character is a single quote not immediately preceded by     */
		/* an odd number of consecutive single quotes.  We toggle the  */
		/* after_quote state to keep track of which single quotes are  */
		/* delimiters and which aren't. */

		if( '\'' == c )
		{
			if( TRUE == after_quote )
				after_quote = FALSE;
			else
				after_quote = TRUE;
		}
		else
		{
			if( TRUE == after_quote )
				finished = TRUE;
			else
			{
				after_quote = FALSE;
				if( EOF == c )
				{
					pT->type = T_error;
					rc = pls_append_msg( pT, "Unterminated string "
						"or character literal" );
					finished = TRUE;
				}
			}
		}

		if( TRUE == finished )
		{
			/* Since we have read one character past the end, we */
			/* put the last character back into the input stream */

			(void) s_ungetc( s, c );
		}
		else
		{
			if( count < LOCAL_BUFLEN )
				buf[ count++ ] = c;
			else
			{
				/* flush the buffer and start over */

				total_count += count;
				buf[ count ] = '\0';
				if( pls_append_text( pT, buf ) != OKAY )
				{
					rc = ERROR_FOUND;
					finished = TRUE;
				}
				buf[ 0 ] = c;
				count = 1;
			}
		}
	} while( FALSE == finished );

	if( count > 0 )
	{
		/* flush the last buffer */

		total_count += count;
		buf[ count ] = '\0';
		if( pls_append_text( pT, buf ) != OKAY )
			return ERROR_FOUND;
	}

	if( pT->type != T_error )
	{
		if( 3 == total_count )
			pT->type = T_char_lit;
		else if( 4 == total_count &&
				 '\'' == pT->buf[ 1 ] && '\'' == pT->buf[ 2 ] )
			pT->type = T_char_lit;
		else
			pT->type = T_string_lit;
	}

	return rc;
}

/*****************************************************************
 get_punct -- build a punctuation token.  In some cases we read
 ahead to see if we have a two-character token like '!='.
 ****************************************************************/
static int get_punct( Pls_tok * pT, Sfile s, int c )
{
	int rc = OKAY;
	int nextc;

	ASSERT( pT != NULL );

	pT->buf[ 0 ] = c;
	pT->buf[ 1 ] = '\0';

	switch( c )
	{
		case '+' :
			pT->type = T_plus;
			break;
		case '(' :
			pT->type = T_lparens;
			break;
		case ')' :
			pT->type = T_rparens;
			break;
		case ';' :
			pT->type = T_semicolon;
			break;
		case '%' :
			pT->type = T_percent;
			break;
		case ',' :
			pT->type = T_comma;
			break;
		case '@' :
			pT->type = T_at_sign;
			break;
		case '\'' :
			rc = get_squote( pT, s );
			break;
		case '"' :
			rc = get_dquote( pT, s );
			break;
		case '*' :
			nextc = s_getc( s );
			if( '*' == nextc )
			{
				pT->buf[ 1 ] = '*';
				pT->buf[ 2 ] = '\0';
				pT->type = T_expo;
			}
			else
			{
				/* In this and similar cases: We skip the error checking  */
				/* for s_ungetc() because, having just read a character,  */
				/* we know we can safely unget it.  This shortcut relies  */
				/* on knowledge of how s_ungetc() is implemented.  To be  */
				/* completely scrupulous we should check the return code. */

				(void) s_ungetc( s, nextc );
				pT->type = T_star;
			}
			break;
		case '-' :
			nextc = s_getc( s );
			if( '-' == nextc )
				rc = get_hyphen_comment( pT, s );
			else
			{
				(void) s_ungetc( s, nextc );
				pT->type = T_minus_sign;
			}
			break;
		case '<' :
			nextc = s_getc( s );
			if( '>' == nextc )
			{
				pT->buf[ 1 ] = '>';
				pT->buf[ 2 ] = '\0';
				pT->type = T_not_equal;
			}
			else if( '=' == nextc )
			{
				pT->buf[ 1 ] = '=';
				pT->buf[ 2 ] = '\0';
				pT->type = T_less_equal;
			}
			else if( '<' == nextc )
			{
				pT->buf[ 1 ] = '<';
				pT->buf[ 2 ] = '\0';
				pT->type = T_left_label;
			}
			else
			{
				(void) s_ungetc( s, nextc );
				pT->type = T_less;
			}
			break;
		case '!' :
			nextc = s_getc( s );
			if( '=' == nextc )
			{
				pT->buf[ 1 ] = '=';
				pT->buf[ 2 ] = '\0';
				pT->type = T_not_equal;
			}
			else
			{
				(void) s_ungetc( s, nextc );
				rc = pls_append_msg( pT, "'!' not followed by '='" );
				pT->type = T_error;
			}
			break;
		case '~' :
			nextc = s_getc( s );
			if( '=' == nextc )
			{
				pT->buf[ 1 ] = '=';
				pT->buf[ 2 ] = '\0';
				pT->type = T_tilde;
			}
			else
			{
				(void) s_ungetc( s, nextc );
				rc = pls_append_msg( pT, "'~' not followed by '='" );
				pT->type = T_error;
			}
			break;
		case '^' :
			nextc = s_getc( s );
			if( '=' == nextc )
			{
				pT->buf[ 1 ] = '=';
				pT->buf[ 2 ] = '\0';
				pT->type = T_hat;
			}
			else
			{
				(void) s_ungetc( s, nextc );
				rc = pls_append_msg( pT, "'^' not followed by '='" );
				pT->type = T_error;
			}
			break;
		case '>' :
			nextc = s_getc( s );
			if( '=' == nextc )
			{
				pT->buf[ 1 ] = '=';
				pT->buf[ 2 ] = '\0';
				pT->type = T_greater_equal;
			}
			else if( '>' == nextc )
			{
				pT->buf[ 1 ] = '>';
				pT->buf[ 2 ] = '\0';
				pT->type = T_right_label;
			}
			else
			{
				(void) s_ungetc( s, nextc );
				pT->type = T_greater;
			}
			break;
		case ':' :
			nextc = s_getc( s );
			if( '=' == nextc )
			{
				pT->buf[ 1 ] = '=';
				pT->buf[ 2 ] = '\0';
				pT->type = T_assignment;
			}
			else
			{
				(void) s_ungetc( s, nextc );
				pT->type = T_colon;
			}
			break;
		case '=' :
			nextc = s_getc( s );
			if( '>' == nextc )
			{
				pT->buf[ 1 ] = '>';
				pT->buf[ 2 ] = '\0';
				pT->type = T_arrow;
			}
			else
			{
				(void) s_ungetc( s, nextc );
				pT->type = T_equals;
			}
			break;
		case '.' :
			nextc = s_getc( s );
			if( '.' == nextc )
			{
				pT->buf[ 1 ] = '.';
				pT->buf[ 2 ] = '\0';
				pT->type = T_range_dots;
			}
			else if( isdigit( (unsigned char) nextc ) )
			{
				(void) s_ungetc( s, nextc );
				(void) s_ungetc( s, '.' );
				rc = get_number( pT, s );
			}
			else
			{
				(void) s_ungetc( s, nextc );
				pT->type = T_dot;
			}
			break;
		case '|' :
			nextc = s_getc( s );
			if( '|' == nextc )
			{
				pT->buf[ 1 ] = '|';
				pT->buf[ 2 ] = '\0';
				pT->type = T_bars;
			}
			else
			{
				(void) s_ungetc( s, nextc );
				rc = pls_append_msg( pT, "'|' not followed by '|'" );
				pT->type = T_error;
			}
			break;
		case '/' :
			nextc = s_getc( s );
			if( '*' == nextc )
				rc = get_c_comment( pT, s );
			else
			{
				(void) s_ungetc( s, nextc );
				pT->buf[ 0 ] = '/';
				pT->buf[ 1 ] = '\0';
				pT->type = T_virgule;
			}
			break;
		default :
			rc = pls_append_msg( pT, "Unrecognized punctuation character" );
			pT->type = T_error;
			break;
	}

	pT->buflen = strlen( pT->buf );
	return rc;
}

/******************************************************************
 get_whitespace -- collect whitespace characters into a token
 *****************************************************************/
static int get_whitespace( Pls_tok * pT, Sfile s, int c )
{
	int rc = OKAY;
	int finished = FALSE;
	size_t count = 1;	/* we already have the first character */
	char buf[ LOCAL_BUFLEN + 1 ];

	ASSERT( pT != NULL );

	pT->type = T_whitespace;
	buf[ 0 ] = c;
	do
	{
		c = s_getc( s );

		if( isspace( (unsigned char) c ) )
		{
			if( count >= LOCAL_BUFLEN )
			{
				/* buffer is full; flush it, start over */

				buf[ LOCAL_BUFLEN ] = '\0';
				rc = pls_append_text( pT, buf );
				if( rc != OKAY )
					finished = TRUE;
				count = 0;
			}
			buf[ count ] = c;
			++count;
		}
		else
		{
			finished = TRUE;
			(void) s_ungetc( s, c );
		}
	} while( FALSE == finished );

	if( OKAY == rc )
	{
		/* flush the buffer one last time */

		buf[ count ] = '\0';
		rc = pls_append_text( pT, buf );
	}

	return rc;
}

/******************************************************************
 get_c_comment -- collect characters from a C-style comment
 (including the comment delimiters themselves).  We presume that
 the first delimiter has already been read.
 *****************************************************************/
static int get_c_comment( Pls_tok * pT, Sfile s )
{
	int rc = OKAY;
	int c;
	int after_asterisk = FALSE;		/* a boolean */
	int finished = FALSE;
	size_t count = 2;	/* we already have the first 2 characters */
	char buf[ LOCAL_BUFLEN + 1 ] = "/*";

	ASSERT( pT != NULL );

	pT->type = T_remark;

	do
	{
		c = s_getc( s );

		if( EOF == c )
		{
			(void) s_ungetc( s, EOF );
			pT->type = T_error;
			finished = TRUE;
			rc = pls_append_msg( pT, "Unterminated C-style token" );
		}
		else
		{
			if( TRUE == preserving )
			{
				/* Append the character to the buffer */

				if( count >= LOCAL_BUFLEN )
				{
					/* buffer is full; flush it, start over */

					buf[ LOCAL_BUFLEN ] = '\0';
					rc = pls_append_text( pT, buf );
					if( rc != OKAY )
						finished = TRUE;
					count = 0;
				}
				buf[ count ] = c;
				++count;
			}
		}

		/* See if we have reached the end of the comment */

		if( '*' == c )
			after_asterisk = TRUE;
		else
		{
			if( '/' == c && TRUE == after_asterisk )
				finished = TRUE;
			after_asterisk = FALSE;
		}
	} while( FALSE == finished );

	if( FALSE == preserving )
	{
		pT->buf[ 0 ] = '\0';
		pT->buflen   = 0;
	}
	else if( OKAY == rc && count > 0 )
	{
		/* flush the buffer one last time */

		buf[ count ] = '\0';
		rc = pls_append_text( pT, buf );
	}

	return rc;
}

/******************************************************************
 get_dquote -- collect characters from a string in double quotes
 (including the quotes themselves).  Such a string is a quoted
 identifier, and may not exceed 30 characters, not counting the
 quotation marks.
 *****************************************************************/
static int get_dquote( Pls_tok * pT, Sfile s )
{
	int rc = OKAY;
	int c;
	int finished = FALSE;
	size_t count = 1;	/* we already have the first character */
	char buf[ LOCAL_BUFLEN + 1 ] = "\"";

	ASSERT( pT != NULL );

	pT->type = T_quoted_id;

	do
	{
		c = s_getc( s );

		if( EOF == c )
		{
			(void) s_ungetc( s, EOF );
			pT->type = T_error;
			finished = TRUE;
			rc = pls_append_msg( pT,
				"Unterminated quoted identifier" );
		}
		else
		{
			/* Append the character to the buffer */

			if( count >= LOCAL_BUFLEN )
			{
				/* buffer is full; flush it, start over */

				buf[ LOCAL_BUFLEN ] = '\0';
				rc = pls_append_text( pT, buf );
				if( rc != OKAY )
					finished = TRUE;
				count = 0;
			}
			buf[ count ] = c;
			++count;
		}

		/* See if we have reached the end of the quoted string */

		if( '"' == c )
			finished = TRUE;

	} while( FALSE == finished );

	if( OKAY == rc && count > 0 )
	{
		/* flush the buffer one last time */

		buf[ count ] = '\0';
		rc = pls_append_text( pT, buf );
	}

	if( pls_tok_size( pT ) > 32 )	/* (including quote marks) */
	{
		pT->type = T_error;
		pls_append_msg( pT, "Quoted identifier is too long" );
	}

	return rc;
}

/******************************************************************
 get_hyphen_comment -- collect characters from a hyphen-style
 comment  (including the terminal newline).  We presume that
 the hyphens have already been read.
 *****************************************************************/
static int get_hyphen_comment( Pls_tok * pT, Sfile s )
{
	int rc = OKAY;
	int c;
	int finished = FALSE;
	size_t count = 2;	/* we already have the first 2 characters */
	char buf[ LOCAL_BUFLEN + 1 ] = "--";

	ASSERT( pT != NULL );

	pT->type = T_remark;

	do
	{
		c = s_getc( s );

		if( EOF == c )
		{
			(void) s_ungetc( s, EOF );
			finished = TRUE;
		}
		else
		{
			if( TRUE == preserving )
			{
				/* Append the character to the buffer */

				if( count >= LOCAL_BUFLEN )
				{
					/* buffer is full; flush it, start over */

					buf[ LOCAL_BUFLEN ] = '\0';
					rc = pls_append_text( pT, buf );
					if( rc != OKAY )
						finished = TRUE;
					count = 0;
				}
				buf[ count ] = c;
				++count;
			}
		}

		/* See if we have reached the end of the comment */

		if( '\n' == c )
			finished = TRUE;

	} while( FALSE == finished );

	if( FALSE == preserving )
	{
		pT->buf[ 0 ] = '\0';
		pT->buflen   = 0;
	}
	else if( OKAY == rc && count > 0 )
	{
		/* flush the buffer one last time */

		buf[ count ] = '\0';
		rc = pls_append_text( pT, buf );
	}

	return rc;
}

/* various definitions and declarations for get_number, below */

typedef enum
{
	S_INITIAL,
	S_LEFT_DIGIT,
	S_RIGHT_DIGIT,
	S_E,
	S_SIGN,
	S_EXPO,
	S_DOT,
	S_ERROR,
	S_FINISHED
} State;

typedef enum
{
	E_DIGIT,
	E_DOT,
	E_E,
	E_SIGN,
	E_OTHER
} Event;

#define NR_OF_EVENTS (5)
#define NR_OF_STATES (9)

/* The following 2D array defines the state      */
/* transitions as a function of state and event. */

static const State machine[ NR_OF_STATES ] [ NR_OF_EVENTS ] =
{
	{	/* S_INITIAL - expecting a digit or decimal point */
		S_LEFT_DIGIT,	/* E_DIGIT */
		S_DOT,			/* E_DOT */
		S_ERROR,		/* E_E */
		S_ERROR,		/* E_SIGN */
		S_ERROR			/* E_OTHER */
	},
	{	/* S_LEFT_DIGIT - collecting digits; no decimal point yet */
		S_LEFT_DIGIT,  	/* E_DIGIT */
		S_RIGHT_DIGIT,	/* E_DOT */
		S_E,			/* E_E */
		S_FINISHED,		/* E_SIGN */
		S_FINISHED		/* E_OTHER */
	},
	{	/* S_RIGHT_DIGIT - collecting digits after decimal point */
		S_RIGHT_DIGIT,  /* E_DIGIT */
		S_FINISHED,		/* E_DOT */
		S_E,			/* E_E */
		S_FINISHED,		/* E_SIGN */
		S_FINISHED		/* E_OTHER */
	},
	{	/* S_E - just got e or E; expect sign or digit */
		S_EXPO,  		/* E_DIGIT */
		S_ERROR,		/* E_DOT */
		S_ERROR,		/* E_E */
		S_SIGN,			/* E_SIGN */
		S_ERROR			/* E_OTHER */
	},
	{	/* S_SIGN - got sign of exponent; expect digit */
		S_EXPO,  		/* E_DIGIT */
		S_ERROR,		/* E_DOT */
		S_ERROR,		/* E_E */
		S_ERROR,		/* E_SIGN */
		S_ERROR			/* E_OTHER */
	},
	{	/* S_EXPO - collecting digits of exponent */
		S_EXPO,  		/* E_DIGIT */
		S_FINISHED,		/* E_DOT */
		S_FINISHED,		/* E_E */
		S_FINISHED,		/* E_SIGN */
		S_FINISHED		/* E_OTHER */
	},
	{	/* S_DOT - started with decimal point; expect digit */
		S_RIGHT_DIGIT,  /* E_DIGIT */
		S_ERROR,		/* E_DOT */
		S_ERROR,		/* E_E */
		S_ERROR,		/* E_SIGN */
		S_ERROR			/* E_OTHER */
	},
	{	/* S_ERROR - terminal state -- should have stopped here */
		S_ERROR,  		/* E_DIGIT */
		S_ERROR,		/* E_DOT */
		S_ERROR,		/* E_E */
		S_ERROR,		/* E_SIGN */
		S_ERROR			/* E_OTHER */
	},
	{	/* S_FINISHED - terminal state -- should have stopped here */
		S_ERROR,  		/* E_DIGIT */
		S_ERROR,		/* E_DOT */
		S_ERROR,		/* E_E */
		S_ERROR,		/* E_SIGN */
		S_ERROR			/* E_OTHER */
	}
};

/******************************************************************
 get_number -- a small finite state machine to collect characters
 for a number, consisting of:

 1. One or more digits, optionally including a single decimal point;

 2. Optional 'E' or 'e', followed by an optional sign and one or
 more digits.

 Note that we allow the sequences of digits to be arbitrarily long.
 We don't try to apply any limits on the size or precision of the
 number represented.  We leave that as an issue for semantic
 analysis rather than lexical analysis.
 *****************************************************************/
static int get_number( Pls_tok * pT, Sfile s )
{
	int rc = OKAY;
	int c;
	State state = S_INITIAL;
	Event event;
	size_t count = 0;
	char buf[ LOCAL_BUFLEN + 1 ];

	while( state != S_FINISHED && state != S_ERROR )
	{
		c = s_getc( s );

		/* categorize the new character as an event */

		if( isdigit( (unsigned char) c ) )
			event = E_DIGIT;
		else if( '.' == c )
			event = E_DOT;
		else if( 'E' == c || 'e' == c )
			event = E_E;
		else if( '-' == c || '+' == c )
			event = E_SIGN;
		else
			event = E_OTHER;


		/* decide on the next state by a table lookup */

		state = machine[ state ][ event ];

		if( S_FINISHED == state || S_ERROR == state )
		{
			/* We've read one character past the */
			/* end of the token, so put it back  */

			(void) s_ungetc( s, c );
		}
		else
		{
			/* Append the character to the buffer */

			if( count >= LOCAL_BUFLEN )
			{
				/* buffer is full; flush it, start over */

				buf[ LOCAL_BUFLEN ] = '\0';
				rc = pls_append_text( pT, buf );
				if( rc != OKAY )
					state = S_ERROR;
				count = 0;
			}
			buf[ count ] = c;
			++count;
		}
	}

	if( OKAY == rc && count > 0 )
	{
		/* flush the buffer one last time */

		buf[ count ] = '\0';
		rc = pls_append_text( pT, buf );
	}

	if( S_ERROR == state )
	{
		if( OKAY == rc )
		{
			pT->type = T_error;
			rc = pls_append_msg( pT, "Invalid numeric literal" );
		}
	}
	else
		pT->type = T_num_lit;

	return rc;
}
