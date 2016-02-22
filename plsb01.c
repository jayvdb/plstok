/* plsb01.c -- low-level routines for managing doubly-linked lists of
   line elements.  Each element may represents a token and, optionally,
   some spacing to be inserted in front of the token.

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

static Toknode * pFree;

static Toknode * alloc_toknode( Pls_tok * pT );
static void scavenge_toknodes( void * dummy );

/*********************************************************************
 extend_toklist -- append a Toknode to a Toklist.
 ********************************************************************/
int extend_toklist( Toklist * pLL, Pls_tok * pT )
{
	Toknode * pTN;

	/* sanity checks */

	ASSERT( pLL != NULL );
	ASSERT( pT  != NULL );
	if( NULL == pLL || NULL == pT )
		return ERROR_FOUND;

	pTN = alloc_toknode( pT );
	if( NULL == pTN )
		return ERROR_FOUND;

	if( NULL == pLL->pLast )
	{
		/* list empty?  Direct both pointers to the new node */

		pLL->pLast  = pTN;
		pLL->pFirst = pTN;
	}
	else
	{
		/* list not empty -- juggle pointers, append to list */

		ASSERT( NULL == pLL->pLast->pNext );
		pLL->pLast->pNext = pTN;
		pTN->pPrev = pLL->pLast;
		pLL->pLast = pTN;
	}
	return OKAY;
}

/*********************************************************************
 truncate_toklist -- remove the last Toknode on a list and return a
 pointer to the Pls_tok (or NULL if the list is empty).
 ********************************************************************/
Pls_tok * truncate_toklist( Toklist * pTL )
{
	Toknode * pTN;
	Pls_tok * pT;

	ASSERT( pTL != NULL );
	if( NULL == pTL || NULL == pTL->pLast )
		return NULL;

	ASSERT( pTL->pFirst != NULL );
	ASSERT( NULL == pTL->pLast->pNext );

	/* detach the last Toknode from the list */

	pTN = pTL->pLast;
	pTL->pLast = pTN->pPrev;
	if( pTL->pLast != NULL )
		pTL->pLast->pNext = NULL;
	pTN->pPrev = NULL;

	/* detach the Pls_tok from the Toknode */

	pT = pTN->pT;
	pTN->pT = NULL;	/* not strictly necessary */

	/* discard the Toknode by sticking it on the free list */

	pTN->pNext = pFree;
	pFree = pTN;

	return pT;
}

/*********************************************************************
 init_toklist -- initialize a Toklist with a Toknode, or, if pT is
 NULL, make it empty.  We assume that the Toklist initially contains
 garbage.  If it contains pointers to  any Toknodes, we'll leak memory.
 ********************************************************************/
int init_toklist( Toklist * pTL, Pls_tok * pT )
{
	int rc = OKAY;
	Toknode * pTN;

	/* sanity checks */

	ASSERT( pTL != NULL );
	if( NULL == pTL )
		return ERROR_FOUND;

	if( NULL == pT )
		pTN = NULL;
	else
	{
		pTN = alloc_toknode( pT );
		if( NULL == pTN )
			rc = ERROR_FOUND;
	}

	pTL->pFirst = pTL->pLast = pTN;

	return rc;
}

/*********************************************************************
 empty_Toklist -- discards all the Toknodes in the list, leaving
 the list empty.
 ********************************************************************/
void empty_toklist( Toklist * pTL )
{
	Toknode * pTN;

	ASSERT( pTL != NULL );
	if( NULL == pTL )
		return;

	if( NULL == pTL->pFirst )
	{
		/* already empty? do nothing */

		ASSERT( NULL == pTL->pLast );
		return;
	}

	/* Make sure the list looks superficially healthy.  We don't */
	/* trouble ourselves to make sure that the list is actually  */
	/* intact, i.e. that the chain leads from the first to the   */
	/* last, or that the backwards pointers are all valid.       */

	ASSERT( pTL->pLast != NULL );
	ASSERT( NULL == pTL->pLast->pNext );
	ASSERT( NULL == pTL->pFirst->pPrev );

	/* Free the token attached to each node */

	for( pTN = pTL->pFirst; pTN != NULL; pTN = pTN->pNext )
		pls_free_tok( &(pTN->pT) );

	/* transfer all the Toknodes to the free list */
	/* (where we only use the forwards pointers)  */

	pTL->pLast->pNext = pFree;
	pTL->pLast = NULL;
	pFree = pTL->pFirst;
	pTL->pFirst = NULL;
}

/*********************************************************************
 alloc_toknode -- allocate and construct a Toknode; from the free
 list if possible, from the free store if necessary.
 *********************************************************************/
static Toknode * alloc_toknode( Pls_tok * pT )
{
	Toknode * pTN;

	/* sanity check */

	ASSERT( pT != NULL );
	if( NULL == pT )
		return NULL;

	// FIXME: the memory manager is seg faulting.
	// given the average size of PL/SQL code, it would be better
	// to use a simple algorithm that isnt very memory efficient during
	// processing, but cleans up after itself when the tokenised document
	// is no longer needed.
	pFree = NULL;

	/* allocate a Toknode */

	if( NULL == pFree )
		pTN = allocMemory( sizeof( Toknode ) );
	else
	{
		static int first_time = TRUE;

		if( TRUE == first_time )
		{
			registerMemoryPool( scavenge_toknodes, NULL );
			first_time = FALSE;
		}
		pTN = pFree;
		pFree = pFree->pNext;
	}

	/* initialize it */

	if( pTN != NULL )
	{
		pTN->pNext  = NULL;
		pTN->pPrev  = NULL;
		pTN->type   = pT->type;
		pTN->spacer = 0;
		pTN->lf     = FALSE;
		pTN->indent_change = 0;
		pTN->size   = pls_tok_size( pT );
		pTN->pT     = pT;
	}

	return pTN;
}

/**********************************************************************
 free_toknode -- deallocate a Toknode.  First we free the token, then
 we stick the Toknode on the free list.
 *********************************************************************/
void free_toknode( Toknode ** ppTN )
{
	Toknode * pTN;

	/* sanity checks */

	ASSERT( ppTN != NULL );
	if( NULL == ppTN )
		return;

	pTN = *ppTN;
	*ppTN = NULL;

	ASSERT( pTN != NULL );
	if( NULL == pTN )
		return;

	/* deallocate the token, if any */

	ASSERT( pTN->pT != NULL );
	if( pTN->pT != NULL )
		pls_free_tok( &(pTN->pT) );

	/* stick the Toknode on the free list (we use */
	/* only the forward pointer for this purpose) */

	pTN->pNext = pFree;
	pFree = pTN;
	return;
}

/***********************************************************************
 scavenge_toknodes -- return all the Toknodes on the free list to the
 free store.  We don't use the dummy parameter; it's only there so that
 we can install this function as a memory scavenger.
 **********************************************************************/
static void scavenge_toknodes( void * dummy )
{
	Toknode * pNext;
	Toknode * pTN;

	pTN = pFree;
	pFree = NULL;

	while( pTN != NULL )
	{
		pNext = pTN->pNext;
		freeMemory( &pTN );
		pTN = pNext;
	}
}
