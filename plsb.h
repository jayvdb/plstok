/* plsb.h -- header for PL/SQL beautifier.

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

#ifndef PLSB_H
#define PLSB_H

struct Toknode_;
typedef struct Toknode_ Toknode;

/* The following struct is a node in a doubly-linked list of tokens.  */

/* The type member merely repeats the type from the associated token, */
/* in order to eliminate a layer of indirection when traversing a     */
/* list looking for various kinds of tokens. */

/* The spacer member represents the number of separating spaces to    */
/* be inserted between this token and the previous one. */

struct Toknode_
{
	Toknode * pNext;
	Toknode * pPrev;
	Pls_token_type type;
	int spacer;
	int lf;		/* boolean; whether issue a line feed before token */
	int indent_change; 	/* how much to change indentation, if at all */
	size_t size;
	Pls_tok * pT;
};

typedef struct
{
	Toknode * pFirst;
	Toknode * pLast;
} Toklist;

typedef enum
{
	P_never,
	P_seldom,
	P_sometimes,
	P_usually,
	P_always
} Probability;

/* The following structure is a home for miscellaneous global     */
/* variables.  There should be only on instance of a Pls_globals. */

typedef struct
{
	int indentation;
	const char * indent_string;
	const char * soft_indent_string;
} Pls_globals;
extern Pls_globals pls_globals;

/* Enum for different kinds of syntax to parse: */

typedef enum
{
	ST_none,			/* outside of any specially parsed context */
	ST_select,
	ST_insert,
	ST_delete,
	ST_update,
	ST_fetch,
	ST_cursor
} Syntax_type;

/* States for a finite state machine: */

typedef enum
{
	S_none,			/* outside of any specially parsed context */
	S_column_list_a, /* starting a column list */
	S_column_list_b, /* within a column list */
	S_column_list_c, /* after a column list */
	S_connect,      /* starting CONNECT clause */
	S_connect_clause,  /* amid CONNECT clause */
	S_cursor,       /* between CURSOR and SELECT */
	S_delete,       /* immediately after DELETE */
	S_fetch,        /* immediately after FETCH */
	S_for,          /* immediately after FOR */
	S_for_update,	/* starting FOR UPDATE clause */
	S_from,			/* immediately after FROM */
	S_from_list,	/* amid table list of FROM clause */
	S_group,        /* starting a GROUP BY clause */
	S_group_list,   /* amid GROUP BY expression */
	S_having,		/* starting a HAVING clause */
	S_having_list,  /* amid a HAVING condition */
	S_insert,       /* immediately after INSERT */
	S_intersect,    /* starting an intersection */
	S_into,			/* immediately after INTO */
	S_into_list,	/* amid objects of INTO clause */
	S_minus,        /* starting a minus */
	S_nowait,		/* immediately after NOWAIT */
	S_of,           /* immediately after OF */
	S_of_list,      /* amid FOR UPDATE OF list */
	S_order,        /* starting an ORDER BY clause */
	S_order_list,   /* amid an ORDER BY list */
	S_select,		/* immediately after SELECT */
	S_select_list,	/* amid selected items */
	S_set,          /* immediately after SET */
	S_set_comma,    /* after comma after subquery in a SET clause */
	S_set_list,     /* amid a SET clause */
	S_set_subquery, /* after a subquery in a SET clause */
	S_start,		/* immediately after START */
	S_start_clause, /* amid START clause */
	S_subquery,     /* within a subquery */
	S_union,		/* starting a union */
	S_update,       /* immediately after UPDATE */
	S_values,       /* immediately after VALUES */
	S_values_list_a, /* starting a list of values */
	S_values_list_b, /* within a list of values */
	S_values_list_c, /* finished a list of values */
	S_where,		/* immediately after WHERE */
	S_where_list	/* amid conditions in WHERE clause */
} S_state;

struct Syntax_level;
typedef struct Syntax_level Syntax_level;

struct Syntax_level
{
	Syntax_type s_type;
	S_state state;
	int indents_count;
	int parens_count;
	Syntax_level * pNext;
};

extern Syntax_level curr_level;

int edit_syntax( Toklist * pTL );

int plsb_init( void );
int plsb_open( Sfile sfile );
int get_logical_line( Toklist * pTL );
int write_logical_line( Toklist * pTL );
void exit_all_levels( void );
int push_level( void );
void pop_level( void );
void defer_unindent( int how_many );
void add_indent( Toknode * pTN, S_state state );
void reduce_indent( Toknode * pTN, S_state state );

int  do_select_syntax( Toknode * pTN );
int  do_insert_syntax( Toknode * pTN );
int  do_update_syntax( Toknode * pTN );
int  do_cursor_syntax( Toknode * pTN );
int  do_fetch_syntax( Toknode * pTN );

void plsb_close( void );
Probability is_final( Pls_token_type type );
Probability is_first( Pls_token_type type );
int need_space( Pls_token_type first, Pls_token_type second );
Probability need_indent( Pls_token_type type );
int need_unindent( Pls_token_type type );

int extend_toklist( Toklist * pLL, Pls_tok * pT );
Pls_tok * truncate_toklist( Toklist * pTL );
int init_toklist( Toklist * pLL, Pls_tok * pT );
void empty_toklist( Toklist * pLL );
void free_toknode( Toknode ** ppTN );

#endif
