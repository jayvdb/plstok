/***************************************************************************
 plstok02.c -- lookup for keywords.  We store a hard-coded array of keywords
 so that we can do a binary search.  This is not as fast as a hash table but
 it is easier to code.

 These functions are designed for complete generality rather than efficiency.
 As noted in the comments, there are various ways to boost performance by
 sacrificing generality, particularly with regard to the collating sequence
 of the character set used.

 The list of reserved words is taken from Oracle's own documentation, in
 the PL/SQL User's Guide and Reference.  However their list does not include
 everything which might plausibly be regarded as a reserved word.  In
 particular it does not include REPLACE.  Since many of the files to be
 processed by plstok will include "CREATE OR REPLACE", I have added REPLACE
 to the list.  Experience may indicate that others should be added in the
 future.  Ideally they should be added in alphabetical order, both in this
 file and in plstok.h.

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
 **************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "util.h"
#include "sfile.h"
#include "plstok.h"

typedef struct
{
	const char * name;
	Pls_token_type type;
} keyword;

const keyword word_array[] =
{
	{ "ABORT", T_abort },
	{ "ACCEPT", T_accept },
	{ "ACCESS", T_access },
	{ "ADD", T_add },
	{ "ALL", T_all },
	{ "ALTER", T_alter },
	{ "AND", T_and },
	{ "ANY", T_any },
	{ "ARRAY", T_array },
	{ "ARRAYLEN", T_arraylen },
	{ "AS", T_as },
	{ "ASC", T_asc },
	{ "ASSERT", T_assert },
	{ "ASSIGN", T_assign },
	{ "AT", T_at },
	{ "AUDIT", T_audit },
	{ "AUTHORIZATION", T_authorization },
	{ "AVG", T_avg },
	{ "BASE_TABLE", T_base_table },
	{ "BEGIN", T_begin },
	{ "BETWEEN", T_between },
	{ "BINARY_INTEGER", T_binary_integer },
	{ "BODY", T_body },
	{ "BOOLEAN", T_boolean },
	{ "BY", T_by },
	{ "CASE", T_case },
	{ "CHAR", T_char },
	{ "CHAR_BASE", T_char_base },
	{ "CHECK", T_check },
	{ "CLOSE", T_close },
	{ "CLUSTER", T_cluster },
	{ "CLUSTERS", T_clusters },
	{ "COLAUTH", T_colauth },
	{ "COLUMN", T_column },
	{ "COMMENT", T_comment },
	{ "COMMIT", T_commit },
	{ "COMPRESS", T_compress },
	{ "CONNECT", T_connect },
	{ "CONSTANT", T_constant },
	{ "CRASH", T_crash },
	{ "CREATE", T_create },
	{ "CURRENT", T_current },
	{ "CURRVAL", T_currval },
	{ "CURSOR", T_cursor },
	{ "DATABASE", T_database },
	{ "DATA_BASE", T_data_base },
	{ "DATE", T_date },
	{ "DBA", T_dba },
	{ "DEBUGOFF", T_debugoff },
	{ "DEBUGON", T_debugon },
	{ "DECLARE", T_declare },
	{ "DECIMAL", T_decimal },
	{ "DEFAULT", T_default },
	{ "DEFINITION", T_definition },
	{ "DELAY", T_delay },
	{ "DELETE", T_delete },
	{ "DELTA", T_delta },
	{ "DESC", T_desc },
	{ "DIGITS", T_digits },
	{ "DISPOSE", T_dispose },
	{ "DISTINCT", T_distinct },
	{ "DO", T_do },
	{ "DROP", T_drop },
	{ "ELSE", T_else },
	{ "ELSIF", T_elsif },
	{ "END", T_end },
	{ "ENTRY", T_entry },
	{ "EXCEPTION", T_exception },
	{ "EXCEPTION_INIT", T_exception_init },
	{ "EXCLUSIVE", T_exclusive },
	{ "EXISTS", T_exists },
	{ "EXIT", T_exit },
	{ "FALSE", T_false },
	{ "FETCH", T_fetch },
	{ "FILE", T_file },
	{ "FLOAT", T_float },
	{ "FOR", T_for },
	{ "FORM", T_form },
	{ "FROM", T_from },
	{ "FUNCTION", T_function },
	{ "GENERIC", T_generic },
	{ "GOTO", T_goto },
	{ "GRANT", T_grant },
	{ "GROUP", T_group },
	{ "HAVING", T_having },
	{ "IDENTIFIED", T_identified },
	{ "IF", T_if },
	{ "IMMEDIATE", T_immediate },
	{ "IN", T_in },
	{ "INCREMENT", T_increment },
	{ "INDEX", T_index },
	{ "INDEXES", T_indexes },
	{ "INDICATOR", T_indicator },
	{ "INITIAL", T_initial },
	{ "INSERT", T_insert },
	{ "INTEGER", T_integer },
	{ "INTERFACE", T_interface },
	{ "INTERSECT", T_intersect },
	{ "INTO", T_into },
	{ "IS", T_is },
	{ "LEVEL", T_level },
	{ "LIKE", T_like },
	{ "LIMITED", T_limited },
	{ "LOCK", T_lock },
	{ "LONG", T_long },
	{ "LOOP", T_loop },
	{ "MAX", T_max },
	{ "MAXEXTENTS", T_maxextents },
	{ "MIN", T_min },
	{ "MINUS", T_minus },
	{ "MLSLABEL", T_mlslabel },
	{ "MOD", T_mod },
	{ "MODE", T_mode },
	{ "MODIFY", T_modify },
	{ "NATURAL", T_natural },
	{ "NATURALN", T_naturaln },
	{ "NEW", T_new },
	{ "NEXTVAL", T_nextval },
	{ "NOAUDIT", T_noaudit },
	{ "NOCOMPRESS", T_nocompress },
	{ "NOT", T_not },
	{ "NOWAIT", T_nowait },
	{ "NULL", T_null },
	{ "NUMBER", T_number },
	{ "NUMBER_BASE", T_number_base },
	{ "OF", T_of },
	{ "OFFLINE", T_offline },
	{ "ON", T_on },
	{ "ONLINE", T_online },
	{ "OPEN", T_open },
	{ "OPTION", T_option },
	{ "OR", T_or },
	{ "ORDER", T_order },
	{ "OTHERS", T_others },
	{ "OUT", T_out },
	{ "PACKAGE", T_package },
	{ "PARTITION", T_partition },
	{ "PCTFREE", T_pctfree },
	{ "PLS_INTEGER", T_pls_integer },
	{ "POSITIVE", T_positive },
	{ "POSITIVEN", T_positiven },
	{ "PRAGMA", T_pragma },
	{ "PRIOR", T_prior },
	{ "PRIVATE", T_private },
	{ "PRIVILEGES", T_privileges },
	{ "PROCEDURE", T_procedure },
	{ "PUBLIC", T_public },
	{ "RAISE", T_raise },
	{ "RANGE", T_range },
	{ "RAW", T_raw },
	{ "REAL", T_real },
	{ "RECORD", T_record },
	{ "REF", T_ref },
	{ "RELEASE", T_release },
	{ "REMR", T_remr },
	{ "RENAME", T_rename },
	{ "REPLACE", T_replace },
	{ "RESOURCE", T_resource },
	{ "RETURN", T_return },
	{ "REVERSE", T_reverse },
	{ "REVOKE", T_revoke },
	{ "ROLLBACK", T_rollback },
	{ "ROW", T_row },
	{ "ROWID", T_rowid },
	{ "ROWLABEL", T_rowlabel },
	{ "ROWNUM", T_rownum },
	{ "ROWS", T_rows },
	{ "ROWTYPE", T_rowtype },
	{ "RUN", T_run },
	{ "SAVEPOINT", T_savepoint },
	{ "SCHEMA", T_schema },
	{ "SELECT", T_select },
	{ "SEPARATE", T_separate },
	{ "SESSION", T_session },
	{ "SET", T_set },
	{ "SHARE", T_share },
	{ "SIZE", T_size },
	{ "SMALLINT", T_smallint },
	{ "SPACE", T_space },
	{ "SQL", T_sql },
	{ "SQLCODE", T_sqlcode },
	{ "SQLERRM", T_sqlerrm },
	{ "START", T_start },
	{ "STATEMENT", T_statement },
	{ "STDDEV", T_stddev },
	{ "SUBTYPE", T_subtype },
	{ "SUCCESSFUL", T_successful },
	{ "SUM", T_sum },
	{ "SYNONYM", T_synonym },
	{ "SYSDATE", T_sysdate },
	{ "TABAUTH", T_tabauth },
	{ "TABLE", T_table },
	{ "TABLES", T_tables },
	{ "TASK", T_task },
	{ "TERMINATE", T_terminate },
	{ "THEN", T_then },
	{ "TO", T_to },
	{ "TRIGGER", T_trigger },
	{ "TRUE", T_true },
	{ "TYPE", T_type },
	{ "UID", T_uid },
	{ "UNION", T_union },
	{ "UNIQUE", T_unique },
	{ "UPDATE", T_update },
	{ "USE", T_use },
	{ "USER", T_user },
	{ "VALIDATE", T_validate },
	{ "VALUES", T_values },
	{ "VARCHAR", T_varchar },
	{ "VARCHAR2", T_varchar2 },
	{ "VARIANCE", T_variance },
	{ "VIEW", T_view },
	{ "VIEWS", T_views },
	{ "WHEN", T_when },
	{ "WHENEVER", T_whenever },
	{ "WHERE", T_where },
	{ "WHILE", T_while },
	{ "WITH", T_with },
	{ "WORK", T_work },
	{ "WRITE", T_write },
	{ "XOR", T_xor },
};

#define KEYWORD_COUNT ( sizeof( word_array ) / sizeof( word_array[ 0 ] ) )

static const keyword * find_keyword( Pls_token_type t );
static int keycmp( const void * p1, const void * p2 );
static int keyword_cmp( const void * p1, const void * p2 );

#ifndef NDEBUG
	static int all_upper( void );
#endif

/***************************************************************************
 pls_keyword -- return token type corresponding to a string, or
 T_identifier if the string is not a keyword.
 **************************************************************************/
Pls_token_type pls_keyword( const char * s )
{
	static int sorted = FALSE;
	const keyword * pWord;

	if( FALSE == sorted )
	{
		/* On the first call we sort the array.  For ASCII this shouldn't */
		/* be necessary because the keywords are hard-coded to be sorted  */
		/* already, but it may be necessary for other character sets,     */
		/* depending in particular on where '_' falls within the          */
		/* collating sequence. */

		qsort( word_array, KEYWORD_COUNT, sizeof( keyword ), keyword_cmp );
		sorted = TRUE;

		#ifndef NDEBUG

			/* Make sure that each keyword is in upper   */
			/* case (or the bsearch() won't work right). */

			ASSERT( all_upper() );

		#endif
	}

	/* Do a binary search for the candidate keyword */

	pWord = (const keyword *) bsearch( s, word_array, KEYWORD_COUNT,
			sizeof( word_array[ 0 ] ), keycmp );

	if( NULL == pWord )
		return T_identifier;
	else
		return pWord->type;
}

/*********************************************************************
 keycmp -- compares a key to a keyword (assuming that the keyword is
 already raised to upper case); to be called by bsearch().
*********************************************************************/
static int keycmp( const void * p1, const void * p2 )
{
   const keyword * pWord;
   const unsigned char * s1;
   const unsigned char * s2;
   int c1;
   int c2;

	/* We skip the usual assertions about non-NULL pointers,   */
	/* since this function should be called only by bsearch(). */

   s1    = (const unsigned char *) p1;
   pWord = (const keyword *) p2;
   s2    = (const unsigned char *) pWord->name;

   for( ; ; s1++, s2++ )
   {
	  if( *s1 == *s2 )
	  {
		 if( '\0' == *s1 )
			return 0;
	  }
	  else
	  {
		 c1 = toupper( *s1 );
		 c2 = *s2;

		 if( c1 > c2 )
			return 1;
		 else if( c1 < c2 )
			return -1;
	  }
   }
}

/***************************************************************************
 keyword_cmp -- compares two keywords (to be called by qsort()).
 **************************************************************************/
static int keyword_cmp( const void * p1, const void * p2 )
{
	const keyword * k1;
	const keyword * k2;

	/* We skip the usual assertions about non-NULL pointers, */
	/* since this function should be called only by qsort()  */

	k1 = (const keyword *) p1;
	k2 = (const keyword *) p2;
	return strcmp( k1->name, k2->name );
}

#ifndef NDEBUG
/**************************************************************************
 all_upper -- scan the keyword array.  If all keywords are in upper case,
 return TRUE; otherwise return FALSE;
 *************************************************************************/
static int all_upper( void )
{
	int i;
	const char * p;

	for( i = KEYWORD_COUNT - 1; i >= 0; --i )	/* for each keyword */
	{
		for( p = word_array[ i ].name; *p != NULL; ++p )
		{
			if( islower( (unsigned char) *p ) )
				return FALSE;
		}
	}
	return TRUE;
}
#endif

/***************************************************************************
 pls_keyword_name -- given a token type, return a pointer to the
 corresponding keyword (or to a null string if it's not a keyword type).
 **************************************************************************/
const char * pls_keyword_name( Pls_token_type t )
{
	const keyword * pWord;

	pWord = find_keyword( t );
	if( NULL == pWord )
		return "";
	else
		return pWord->name;
}

/***************************************************************************
 pls_is_keyword -- given a token type, return TRUE if it represents a
 keyword, and FALSE otherwise.

 For complete generality we do a linear search through the keyword array.
 For better performance we could do a simple range test, assuming that
 the keyword token types are a contiguous series of integers.
 **************************************************************************/
const int pls_is_keyword( Pls_token_type t )
{
	const keyword * pWord;

	pWord = find_keyword( t );
	if( NULL == pWord )
		return FALSE;
	else
		return TRUE;
}

/*********************************************************************
find_keyword -- search the keyword array for a given type value.
Return a pointer to the keyword if found, or NULL if not.

We search the list backwards instead of the more obvious forwards
search on the theory that a comparison of i to zero is likely to be
marginally more efficient than a comparison to KEYWORD_COUNT.

We could do a binary search, but only if we made assumptions about how
token types are assigned, and about the character set used.  In the
interest of portability we do a slower linear search.  If performance
is a problem we can build a second array sorted by token type, and do a
binary search on it.

For even better performance we could add an offset to the type and
use the result as a subscript into the table.  This tactic makes the
further assumption that the keyword token types are a contiguous
series of integers.
*********************************************************************/
static const keyword * find_keyword( Pls_token_type t )
{
	int i;

	for( i = KEYWORD_COUNT - 1; i >= 0; --i )
	{
		if( word_array[ i ].type == t )
			return word_array + i;
	}
	return NULL;
}
