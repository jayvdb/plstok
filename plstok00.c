/* plstok00.c -- low-level routines for constructing and destructing
   PL/SQL tokens and the associated Chunks, and for managing list of
   Chunks to store arbitrarily long character strings.

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
#include "plspriv.h"

/* The following struct is used to continue character strings which */
/* are too long to fit into a token.  We do not add a terminal nul  */
/* within a Chunk. */

typedef struct chunk
{
	struct chunk * pNext;
	size_t len;
	char buf[ CHUNK_SIZE ];
} Chunk;

/* free lists for Chunks and Pls_toks which have been */
/* allocated but which are not currently in use:      */

static Chunk * freechunk = NULL;
static Pls_tok * freetok = NULL;

/* local functions: */

static Chunk * pls_alloc_chunk( void );
static Chunk * extend_chunks( Chunk * pChunk, const char * str );
static void free_chunk( Chunk * pChunk );
static void free_chunk_list( Chunk ** ppChunk );
static void free_all( void * dummy );

/****************************************************************
 pls_alloc_token -- allocate and initialize a token.  Note that
 this initialization is incomplete.  This function is similar to
 a constructor for an abstract base class.
 ***************************************************************/
Pls_tok * pls_alloc_tok( void )
{
	Pls_tok * pT;

	/* allocate from the free list if possible, */
	/* or from the free store if necessary      */

	if( NULL == freetok )
	{
		static int signed_up = FALSE;

		if( !signed_up )
		{
			registerMemoryPool( free_all, NULL );
			signed_up = TRUE;
		}
		pT = allocMemory( sizeof( Pls_tok ) );
	}
	else
	{
		pT = freetok;
		freetok = (Pls_tok *) pT->pChunk;
	}

	if( pT != NULL )
	{
		/* initialize */

		pT->type   = T_none;
		pT->line   = 0;
		pT->col    = 0;
		pT->buflen = 0;
		pT->buf[ 0 ] = '\0';
		pT->pChunk = NULL;
		pT->pLast  = NULL;
		pT->msg    = NULL;
	}

	return pT;
}

/****************************************************************
 pls_tok_size -- return the total length of a token's text,
 including all the Chunks, but not including a terminal nul.
 ***************************************************************/
size_t pls_tok_size( const Pls_tok * pT )
{
	size_t size;
	const Chunk * pChunk;

	ASSERT( pT != NULL );
	if( NULL == pT )
		return 0;

	size = pT->buflen;
	for( pChunk = (Chunk *) pT->pChunk; pChunk != NULL;
		 pChunk = pChunk->pNext )
	{
		ASSERT( pChunk->len <= CHUNK_SIZE );
		size += pChunk->len;
	}

	return size;
}

/****************************************************************
 pls_copy_text: copy the text of a token to a specified buffer,
 up to a specified number of characters (including the terminal
 nul).  Return a pointer to the terminal nul.
 ***************************************************************/
char * pls_copy_text( const Pls_tok * pT, char * p, size_t n )
{
	size_t len;	/* how much to copy at once */
	const Chunk * pChunk;

	/* sanity checks */

	ASSERT( pT != NULL );
	ASSERT( p != NULL );
	if( NULL == p || 0 == n )
		return p;
	else if( NULL == pT )
	{
		*p = '\0';
		return p;
	}

	/* Leave room for the terminal nul */
	/* which we will add at the end    */

	--n;

	/* copy as much as we can from pT->buf[] */

	if( pT->buflen < n )
		len = pT->buflen;
	else
		len = n;
	strncpy( p, pT->buf, len );
	p += len;
	n -= len;

	/* Now copy as much as we can from the Chunks */

	pChunk = (Chunk *) pT->pChunk;

	while( n > 0 && pChunk != NULL )
	{
		if( pChunk->len < n )
			len = pChunk->len;
		else
			len = n;
		strncpy( p, pChunk->buf, n );
		p += len;
		n -= len;

		pChunk = pChunk->pNext;
	}

	*p = '\0';
	return p;
}

/****************************************************************
 pls_append_msg -- append a string to the message.  Since we
 don't expect to do this operation very often, we don't mind the
 overhead of allocating and deallocating memory for it.
 ***************************************************************/
int pls_append_msg( Pls_tok * pT, const char * str )
{
	ASSERT(  pT != NULL );
	ASSERT( str != NULL );

	if( NULL == pT || NULL == str )
		return ERROR_FOUND;
	else if( '\0' == str[ 0 ] )
		return OKAY;

	if( NULL == pT->msg )
	{
		pT->msg = strDup( str );
		if( NULL == pT->msg )
			return ERROR_FOUND;
	}
	else
	{
		char * newmsg;

		newmsg = resizeMemory( pT->msg,
			strlen( pT->msg ) + strlen( str ) + 1 );
		if( NULL == newmsg )
			return ERROR_FOUND;
		else
		{
			strcat( newmsg, str );
			pT->msg = newmsg;
		}
	}
	return OKAY;
}

/****************************************************************
 pls_append_text -- append a string to the buffer, adding Chunks
 as needed.
 ***************************************************************/
int pls_append_text( Pls_tok * pT, const char * str )
{
	int rc = OKAY;

	ASSERT( pT != NULL );
	ASSERT( str != NULL );

	if( NULL == pT || NULL == str )
		return ERROR_FOUND;
	else if( '\0' == str[ 0 ] )
		return OKAY;

	/* If there's any space in the initial buffer, use it */

	if( pT->buflen < PLS_MAX_WORD )
	{
		size_t string_length;
		size_t room;

		ASSERT( NULL == pT->pChunk );

		string_length = strlen( str );
		room = PLS_MAX_WORD - pT->buflen;

		if( room >= string_length )
		{
			/* copy the whole string */

			strcpy( pT->buf + pT->buflen, str );
			str += string_length;
			pT->buflen += string_length;
		}
		else
		{
			/* copy as much as will fit */

			strncpy( pT->buf + pT->buflen, str, room );
			pT->buf[ PLS_MAX_WORD ] = '\0';
			str += room;
			pT->buflen = PLS_MAX_WORD;
		}
	}

	/* If any of the string remains, put it into one or more Chunks */

	if( str[ 0 ] != '\0' )
	{
		Chunk * pLast;

		pLast = (Chunk *) pT->pLast;

		if( NULL == pLast )
		{
			/* Don't have any Chunks yet?  Make one. */

			ASSERT( NULL == pT->pChunk );

			pLast = pls_alloc_chunk();
			if( NULL == pLast )
				return ERROR_FOUND;
			else
			{
				pT->pChunk = (void *) pLast;
				pT->pLast  = (void *) pLast;
			}
		}

		/* Put the rest of the string into Chunks */

		pLast = extend_chunks( pLast, str );
		if( NULL == pLast )
		{
			/* Unable to allocate enough Chunks.  Before we */
			/* bail out, find the last Chunk so that we can */
			/* leave the token in a consistent state. */

			pLast = (Chunk *) pT->pLast;
			ASSERT( pLast != NULL );

			while( pLast->pNext != NULL )
				pLast = pLast->pNext;

			rc = ERROR_FOUND;
		}
		pT->pLast = (void *) pLast;
	}

	return rc;
 }

/****************************************************************
 extend_chunks -- append a string to a Chunk, adding more
 Chunks as needed.  Return a pointer to the last Chunk, or
 NULL if unsuccessful.
 ***************************************************************/
static Chunk * extend_chunks( Chunk * pChunk, const char * str )
{
	size_t to_go;

	ASSERT( pChunk != NULL );
	ASSERT( NULL == pChunk->pNext );	/* better be last in the list */
	ASSERT( str != NULL );

	if( NULL == pChunk || NULL != pChunk->pNext )
		return NULL;
	else if( NULL == str || '\0' == str[ 0 ] )
		return pChunk;

	to_go = strlen( str );

	ASSERT( pChunk->len <= CHUNK_SIZE );

	/* If there's any room left in the first Chunk, use it */

	if( pChunk->len < CHUNK_SIZE )
	{
		size_t len;

		/* determine how much to copy to this Chunk -- namely  */
		/* the space available or the entire string, whichever */
		/* is less */

		len = CHUNK_SIZE - pChunk->len;
		if( len > to_go )
			len = to_go;

		/* copy it, adjust counters and pointer accordingly */

		strncpy( pChunk->buf + pChunk->len, str, len );
		str += len;
		to_go -= len;
		pChunk->len += len;
	}

	/* Allocate as many Chunks as you need to contain the rest */

	while( to_go > 0 )
	{
		size_t len;
		Chunk * pNext;

		/* Proceed to the next Chunk */

		pNext = pls_alloc_chunk();
		if( NULL == pNext )
			return NULL;

		pChunk->pNext = pNext;
		pChunk = pNext;

		/* determine how much to copy */

		if( to_go > CHUNK_SIZE )
			len = CHUNK_SIZE;
		else
			len = to_go;

		/* copy it; adjust counters and pointer */

		strncpy( pChunk->buf, str, len );
		str += len;
		to_go -= len;
		pChunk->len = len;
	}

	return pChunk;
}

/****************************************************************
 pls_write_text -- Write the textual contents of a token to a
 file, including both the initial buffer and any associated
 Chunks.
 ***************************************************************/
void pls_write_text( const Pls_tok * pT, FILE * pF )
{
	ASSERT( pT != NULL );

	if( NULL == pT )
		return;
	else if( 0 == pT->buflen )
	{
		ASSERT( NULL == pT->pChunk );
		return;
	}

	fputs( pT->buf, pF );

	if( pT->pChunk != NULL )
	{
		Chunk * pChunk;

		ASSERT( pT->pLast != NULL );
		ASSERT( PLS_MAX_WORD == pT->buflen );

		pChunk = (Chunk *) pT->pChunk;
		while( pChunk != NULL )
		{
			ASSERT( pChunk->len <= CHUNK_SIZE );

			fwrite( pChunk->buf, pChunk->len, 1, pF );
			pChunk = pChunk->pNext;
		}
	}
}

/****************************************************************
 pls_alloc_chunk -- allocate and initialize a Chunk.
 ***************************************************************/
static Chunk * pls_alloc_chunk( void )
{
	Chunk * pChunk;

	/* allocate from the free list if possible, */
	/* or from the free store if necessary      */

	if( NULL == freechunk )
		pChunk = allocMemory( sizeof( Chunk ) );
	else
	{
		pChunk = freechunk;
		freechunk = freechunk->pNext;
	}

	if( pChunk != NULL )
	{
		/* initialize */

		pChunk->pNext = NULL;
		pChunk->len = 0;
	}

	return pChunk;
}

/****************************************************************
 free_chunk -- deallocate a chunk
 ***************************************************************/
static void free_chunk( Chunk * pChunk )
{
	ASSERT( pChunk != NULL );

	if( NULL != pChunk )
	{
		/* stick on the free list for possible reuse */

		pChunk->pNext = freechunk;
		freechunk = pChunk;
	}
}

/****************************************************************
 free_chunk_list -- deallocate each of the Chunks in a linked list
 ***************************************************************/
static void free_chunk_list( Chunk ** ppChunk )
{
	Chunk * pChunk;
	Chunk * pTemp;

	ASSERT( ppChunk != NULL );

	if( NULL == ppChunk || NULL == *ppChunk )
		return;

	pChunk = *ppChunk;
	*ppChunk = NULL;

	while( pChunk != NULL )
	{
		pTemp = pChunk->pNext;
		free_chunk( pChunk );
		pChunk = pTemp;
	}
}

/****************************************************************
 free_all -- a memory scavenger.  Free all the tokens and Chunks
 on the free lists.  We don't use the dummy parameter -- it's
 only there so we can install it with registerMemoryPool().
 ***************************************************************/
static void free_all( void * dummy )
{
	Pls_tok * pT;
	Pls_tok * pTemp;
	Chunk * pChunk;
	Chunk * pTempChunk;

	ASSERT( NULL == dummy );

	pT = freetok;
	freetok = NULL;

	while( pT != NULL )
	{
		/* Free the token.  Note that we don't free the Chunk list; */
		/* It isn't really a Chunk list any more, since we've       */
		/* hijacked pChunk to use as a pointer to the next token.   */

		pTemp = (Pls_tok *) pT->pChunk;
		freeMemory( pT );
		pT = pTemp;
	}

	pChunk = freechunk;
	freechunk = NULL;

	while( pChunk != NULL )
	{
		pTempChunk = pChunk->pNext;
		freeMemory( pChunk );
		pChunk = pTempChunk;
	}

	return;
}

/****************************************************************
 pls_free_tok -- deallocate a token and all of its Chunks.  We
 deallocate merely by sticking things on a free list so that we
 can reuse them cheaply later.

 For a pointer with which to stitch the list together, we hijack
 the pChunk pointer and use it as a Pls_tok *.
 ***************************************************************/
void pls_free_tok( Pls_tok ** ppT )
{
	Pls_tok * pT;

	if( NULL == ppT || NULL == *ppT )
		return;

	pT = *ppT;
	*ppT = NULL;

	if( pT->pChunk != NULL )
		free_chunk_list( (Chunk **) &pT->pChunk );
	pT->pLast = NULL;				/* not strictly necessary */
	if( pT->msg != NULL )
	{
		freeMemory( pT->msg );
		pT->msg = NULL;				/* not strictly necessary */
	}

	pT->pChunk = (void *) freetok;
	freetok = pT;
}
