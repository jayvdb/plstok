/* plstok.h -- header for routines to tokenize PL/SQL.  Should be preceded
   by sfile.h.

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

#ifndef PLSTOK_H
#define PLSTOK_H

#include <stdio.h>
#include "sfile.h"

typedef enum
{
	T_eof,			/* end of file */
	T_none,         /* should never be returned */
	T_error,
	T_quoted_id,
	T_string_lit,
	T_char_lit,
	T_num_lit,
	T_identifier,
	T_remark,
	T_whitespace,

	/* single-character punctuation (we don't isolate */
	/* single or double quotes as separate tokens):   */

	T_plus,     	/* '+' */
	T_minus_sign,  	/* '-' */
	T_star,     	/* '*' */
	T_virgule,		/* '/' */
	T_equals,   	/* '=' */
	T_less,     	/* '<' */
	T_greater,  	/* '>' */
	T_lparens,  	/* '(' */
	T_rparens,  	/* ')' */
	T_semicolon,    /* ';' */
	T_percent,      /* '%' */
	T_comma,        /* ',' */
	T_dot,          /* '.' */
	T_at_sign,      /* '@' */
	T_colon,        /* ':' */

	/* two-character operators (we don't isolate */
	/* comment delimiters as separate tokens):   */

	T_expo,				/* exponentiation '**' */
	T_not_equal,		/* '!=' or '<>' */
	T_tilde,			/* '~=' */
	T_hat,				/* '^=' */
	T_less_equal,		/* '<=' */
	T_greater_equal,	/* '>=' */
	T_assignment,		/* ':=' */
	T_arrow,			/* '=>' */
	T_range_dots,		/* '..' */
	T_bars,				/* '||' */
	T_left_label,		/* '<<' */
	T_right_label,		/* '>>' */

	/* reserved words: */

	T_abort = 256,
	T_accept,
	T_access,
	T_add,
	T_all,
	T_alter,
	T_and,
	T_any,
	T_array,
	T_arraylen,
	T_as,
	T_asc,
	T_assert,
	T_assign,
	T_at,
	T_audit,
	T_authorization,
	T_avg,
	T_base_table,
	T_begin,
	T_between,
	T_binary_integer,
	T_body,
	T_boolean,
	T_by,
	T_case,
	T_char,
	T_char_base,
	T_check,
	T_close,
	T_cluster,
	T_clusters,
	T_colauth,
	T_column,
	T_comment,
	T_commit,
	T_compress,
	T_connect,
	T_constant,
	T_crash,
	T_create,
	T_current,
	T_currval,
	T_cursor,
	T_database,
	T_data_base,
	T_date,
	T_dba,
	T_debugoff,
	T_debugon,
	T_declare,
	T_decimal,
	T_default,
	T_definition,
	T_delay,
	T_delete,
	T_delta,
	T_desc,
	T_digits,
	T_dispose,
	T_distinct,
	T_do,
	T_drop,
	T_else,
	T_elsif,
	T_end,
	T_entry,
	T_exception,
	T_exception_init,
	T_exclusive,
	T_exists,
	T_exit,
	T_false,
	T_fetch,
	T_file,
	T_float,
	T_for,
	T_form,
	T_from,
	T_function,
	T_generic,
	T_goto,
	T_grant,
	T_group,
	T_having,
	T_identified,
	T_if,
	T_immediate,
	T_in,
	T_increment,
	T_index,
	T_indexes,
	T_indicator,
	T_initial,
	T_insert,
	T_integer,
	T_interface,
	T_intersect,
	T_into,
	T_is,
	T_level,
	T_like,
	T_limited,
	T_lock,
	T_long,
	T_loop,
	T_max,
	T_maxextents,
	T_min,
	T_minus,
	T_mlslabel,
	T_mod,
	T_mode,
	T_modify,
	T_natural,
	T_naturaln,
	T_new,
	T_nextval,
	T_noaudit,
	T_nocompress,
	T_not,
	T_nowait,
	T_null,
	T_number,
	T_number_base,
	T_of,
	T_offline,
	T_on,
	T_online,
	T_open,
	T_option,
	T_or,
	T_order,
	T_others,
	T_out,
	T_package,
	T_partition,
	T_pctfree,
	T_pls_integer,
	T_positive,
	T_positiven,
	T_pragma,
	T_prior,
	T_private,
	T_privileges,
	T_procedure,
	T_public,
	T_raise,
	T_range,
	T_raw,
	T_real,
	T_record,
	T_ref,
	T_release,
	T_remr,
	T_rename,
	T_replace,	/* not, strictly speaking, a reserved word */
	T_resource,
	T_return,
	T_reverse,
	T_revoke,
	T_rollback,
	T_row,
	T_rowid,
	T_rowlabel,
	T_rownum,
	T_rows,
	T_rowtype,
	T_run,
	T_savepoint,
	T_schema,
	T_select,
	T_separate,
	T_session,
	T_set,
	T_share,
	T_size,
	T_smallint,
	T_space,
	T_sql,
	T_sqlcode,
	T_sqlerrm,
	T_start,
	T_statement,
	T_stddev,
	T_subtype,
	T_successful,
	T_sum,
	T_synonym,
	T_sysdate,
	T_tabauth,
	T_table,
	T_tables,
	T_task,
	T_terminate,
	T_then,
	T_to,
	T_trigger,
	T_true,
	T_type,
	T_uid,
	T_union,
	T_unique,
	T_update,
	T_use,
	T_user,
	T_validate,
	T_values,
	T_varchar,
	T_varchar2,
	T_variance,
	T_view,
	T_views,
	T_when,
	T_whenever,
	T_where,
	T_while,
	T_with,
	T_work,
	T_write,
	T_xor
} Pls_token_type;

#define FIRST_KEYWORD T_abort
#define LAST_KEYWORD  T_xor
#define CHUNK_SIZE 104

#define PLS_MAX_WORD 32  /* maximum length of keyword or identifier; */
						 /* includes 2 bytes for the quotation marks */
						 /* surrounding a quoted identifier */
#define PLS_BUFLEN (PLS_MAX_WORD + 1)

struct pls_tok
{
	Pls_token_type type;
	int line;
	int col;
	char buf[ PLS_BUFLEN ];
	size_t buflen;	/* how many characters used, not counting nul */
	void * pChunk;
	void * pLast;
	char  * msg;
};

typedef struct pls_tok Pls_tok;

#ifdef __cplusplus
	extern "C" {
#endif

Pls_tok * pls_next_tok( Sfile s );
void pls_free_tok( Pls_tok ** ppT );
size_t pls_tok_size( const Pls_tok * pT );
char * pls_copy_text( const Pls_tok * pT, char * p, size_t n );
void pls_write_text( const Pls_tok * pT, FILE * pF );
int pls_preserve( void );
int pls_nopreserve( void );
const char * pls_keyword_name( Pls_token_type t );
const int pls_is_keyword( Pls_token_type t );

#ifdef __cplusplus
	};
#endif

#endif
