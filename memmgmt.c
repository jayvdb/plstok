/* memmgmt.c -- a collection of memory-management routines; a wrapper
   for malloc() and free().

   One purpose is to issue a uniform message when memory allocation
   fails.

   Another is to provide a more robust interface, with more stringent
   validations: no allocations of zero bytes, no freeing of a NULL
   pointer.

   Another is to provide a mechanism for freeing memory which is
   allocated but currently unused.  So if malloc() fails, we can
   free whatever memory we can spare and try again.  In theory we
   can avoid some out-of-memory failures.

   But the main purpose is to provide a home for layers of debugging
   code, so that we can detect and track down memory management bugs
   more readily.

   This approach is based on the techniques detailed by Steve Maguire
   in Writing Solid Code (Microsoft Press, 1993, Redmond, WA).

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

/*
  For the debugging version:

  The following byte values were chosen because they are not
  printable and cannot be part of packed decimal data (i.e. COBOL
  COMP-3).  And they are neither binary zeros nor binary ones.  So
  they are unlikely to occur legitimately in long runs of garbage
  data.  If we see them in the wake of a bug, we can recognize them.

  We fill newly allocated memory with NEWGARBAGE.
*/

#define NEWGARBAGE   ('\xFB')
#define POOLMAX      (32)		/* number of pools we can register */

typedef struct
{
    void ( * freeFunc ) ( void * ); /* ptr to memory-freeing function */
    void * genericPtr;              /* ptr to be passed to the above */
} MemoryPool;

/* Note: all pointers in the following array are implicitly
  initialized to NULL.
*/
static MemoryPool memoryPoolList [ POOLMAX ];

static unsigned slotCount   = 0;
static unsigned excessPools = 0;

#ifndef NDEBUG

static int firstTime = TRUE;
static unsigned long allocationCount = 0;
static unsigned long outstandingCount = 0;
static unsigned long maxCount = 0;

static void reportMemory( void );

#endif

/*******************************************************************
 allocMemory -- a wrapper for malloc().

 For the debugging version, we fill newly-allocated memory with an
 arbitrary value which is unlikely to occur legitimately throughout
 a memory block.  If buggy code tries to use the contents of that
 memory before initializing it, the resulting bugs will be more
 predictable and more recognizable than they would be if we just
 left random garbage in there.

 Also we record a running count of the memory allocations, and the
 maximum allocations.

 In the debugging version: at the end of the job we will free all
 memory pools, then run a memory usage report so that we can
 detect memory leaks.
*******************************************************************/
void * allocMemory( size_t size )
{
	void * p;

#ifndef NDEBUG

	/* Note that functions installed by atexit() are installed
       in one order but called in the reverse order.  We install
       reportMemory() first so that it will be called last.
	*/

    if( TRUE == firstTime )
    {
       atexit( reportMemory );
	   atexit( purgeMemoryPools );
       firstTime = FALSE;
    }
    allocationCount++;

#endif

    ASSERT( size != 0 );

    p = malloc( size );

    /* In case of failure we release all memory pools and try again. */

    if( NULL == p && slotCount > 0 )
    {
		purgeMemoryPools();
        p = malloc( size );
	}

	if( NULL == p )
	{
		fprintf( stderr,
				 "\nallocMemory: unable to allocate %lu bytes\n",
				 (unsigned long) size );
		return NULL;
	}
	else
	{

#ifndef NDEBUG

		memset( p, NEWGARBAGE, size );
		outstandingCount++;
		if( outstandingCount > maxCount )
			maxCount = outstandingCount;

#endif

		return p;
	}
}

/*******************************************************************
 allocNulMemory -- a wrapper for calloc().

 This function is similar to allocMemory, except that it copies the
 interface from the standard function calloc(), and it fills the
 newly-allocated memory block with '\0' instead of '\xFB'.
*******************************************************************/
void * allocNulMemory( size_t nitems, size_t size )
{
	void * p;

#ifndef NDEBUG

	/* Note that functions installed by atexit() are installed
	   in one order but called in the reverse order.  We install
	   reportMemory() first so that it will be called last.
	*/

	if( TRUE == firstTime )
	{
	   atexit( reportMemory );
	   atexit( purgeMemoryPools );
	   firstTime = FALSE;
	}
	allocationCount++;

#endif

	ASSERT( nitems != 0 );
	ASSERT( size != 0 );

	p = calloc( nitems, size );

	/* In case of failure we release all memory pools and try again. */

	if( NULL == p && slotCount > 0 )
	{
		purgeMemoryPools();
		p = calloc( nitems, size );
	}

	if( NULL == p )
	{
		fprintf( stderr,
				 "\nallocNulMemory: unable to allocate %lu bytes\n",
				 (unsigned long) ( nitems * size ) );
		return NULL;
	}
	else
	{

#ifndef NDEBUG

		outstandingCount++;
		if( outstandingCount > maxCount )
			maxCount = outstandingCount;

#endif

		return p;
	}
}

/*******************************************************************
 resizeMemory -- resizes a block of dynamically allocated memory.

 First we call realloc().  If this call fails, we invoke any
 available memory scavengers and try again.  Return the result from
 realloc().

 Since the number of outstanding allocations doesn't change, we
 don't update the counter.

 This function is not intended to be a replacement for realloc().  In
 particular it does not accept a NULL pointer or a zero size.
*******************************************************************/
void * resizeMemory( void * pOld, size_t size )
{
	void * p;

	ASSERT( pOld != NULL );
	ASSERT( size != 0 );
	if( NULL == pOld || 0 == size )
		return NULL;

	p = realloc( pOld, size );

	/* In case of failure we release all memory pools and try again. */

	if( NULL == p && slotCount > 0 )
	{
		purgeMemoryPools();
		p = realloc( pOld, size );
	}

	if( NULL == p )
		fprintf( stderr,
			 "\nresizeMemory: unable to allocate %lu bytes\n",
			 (unsigned long) size );

	return p;
}

/********************************************************************
 freeMemory -- a wrapper for free().  So far nothing special.  We
 don't try to fill the memory with garbage because, in the absence
 of additional machinery, we don't know how big the block is.

 For the debug version we decrement the allocation count.
*********************************************************************/
void freeMemory( void * pMem )
{
    ASSERT( NULL != pMem );

    free( pMem );
#ifndef NDEBUG

	ASSERT( outstandingCount > 0 );
    outstandingCount--;

#endif
}

/*********************************************************************
strDup -- a replacement for the standard C function strdup(), using
	allocMemory() instead of malloc().
*********************************************************************/
char * strDup( const char * s )
{
   char * p;

   ASSERT( s != NULL );

   /* despite the above assertion: if we receive */
   /* a NULL pointer, we create an empty string. */

   if( NULL == s )
      s = "";

   p = allocMemory( strlen( s ) + 1 );
   if( p != NULL )
      strcpy( p, s );

   return p;
}

/*******************************************************************
 registerMemoryPool -- stores for later use: a ptr to a memory-
					   freeing function and a void ptr to be
					   passed to it.  If we don't have room to store
					   them, we discard them.

 So what is the purpose of the void pointer?

 Suppose you have multiple instances of struct FOOBAR, and each
 FOOBAR has a memory pool associated with it.  When you construct
 each FOOBAR, you can register the memory-freeing function, together
 with a pointer to that particular FOOBAR.  When you destruct that
 FOOBAR, you should call that function and pass it the appropriate
 pointer so that it will free the memory associated just with that
 particular FOOBAR.  Then un-register the function for that FOOBAR.

 If we ever need to free memory from within allocMemory(), we will
 free memory for all the registered FOOBARs.

 Most typically, however, the void pointer will be NULL.

 The current implementation stores the pairs of function and void pointers 
 in a simple array of fixed size.  If that turns out to be too confining, 
 we can use some other approach, such as a linked list or tree.
*******************************************************************/
void registerMemoryPool( void (* pFunction) (void *), void * p )
{
    MemoryPool * pMP;
	MemoryPool * pOpenSlot = NULL;
	int i;

    ASSERT( pFunction != NULL );

    /* Scan all the pools.  Maybe this pool is already registered.  If
       so, the debug version aborts, but the production version just 
	   returns without complaint.

       If the pool is not already registered, look for an open slot for it.
    */

    for( pMP = memoryPoolList, i = slotCount; 
		 i > 0;
		 pMP++, i-- )
    {
        if( NULL == pMP->freeFunc )
        {
            ASSERT( NULL == pMP->genericPtr );
			pOpenSlot = pMP;
			break;
        }
        else 
		{
			ASSERT( pMP->freeFunc != pFunction || pMP->genericPtr != p );
			if(     pMP->freeFunc == pFunction && pMP->genericPtr == p )
            	return;
		}
    }

    /* if we haven't found an open slot yet, bump the counter
       (and check for overflow)
    */

    if( NULL == pOpenSlot )
    {
        if( slotCount >= POOLMAX )
		{
			/* no available slot -- forget it */

            excessPools++;
            return;
        }
        else
		{
			pOpenSlot = memoryPoolList + slotCount;
            slotCount++;
		}
    }

    /* Having picked a slot, store the pointers in it */

    pOpenSlot->freeFunc   = pFunction;
    pOpenSlot->genericPtr = p;

    return;
}

/*******************************************************************
 UnRegisterMemoryPool -- removes a memory pool from the list.
*******************************************************************/
void unRegisterMemoryPool( void (* pFunction) (void *), const void * p )
{
    MemoryPool *pMP;
    int i;

    ASSERT( pFunction != NULL );

    /* scan the pools, looking for the one we're supposed to remove. */ 

    pMP = memoryPoolList;
    for( i = slotCount; i > 0; i-- )
    {
		if( pMP->freeFunc   == pFunction &&
            pMP->genericPtr == p )
        {
            pMP->freeFunc   = NULL;
            pMP->genericPtr = NULL;
            return;
        }
        else
            pMP++;
    }

    /* if we haven't found the specified pool, we must have run out
	   of slots at some point -- or else we're being asked to
       unregister a pool that was never registered in the first place.
    */

    ASSERT( slotCount == POOLMAX );

    return;
}

/*******************************************************************
 purgeMemoryPools -- calls all registered routines for freeing memory
*******************************************************************/
void purgeMemoryPools( void )
{
    MemoryPool *pMP;
    int i;

    pMP = memoryPoolList;
    for( i = slotCount; i > 0; i-- )
    {
        if( NULL != pMP->freeFunc )
        {
            pMP->freeFunc ( pMP->genericPtr );
        }
		pMP++;
    }

    return;
}

#ifndef NDEBUG

/********************************************************************
 reportMemory -- reports memory usage.
********************************************************************/
static void reportMemory( void )
{
    fprintf( stderr,
             "\nMaximum memory pools registered: %u\n", slotCount );

    if( excessPools != 0 )
        fprintf( stderr,
                 "Memory pools which could not be registered: %u\n",
                 excessPools );

    fprintf( stderr,
             "Total memory allocations: %lu\n", allocationCount );

	fprintf( stderr,
             "Maximum outstanding memory allocations: %lu\n",
             maxCount );

    if( outstandingCount != 0 )
        fprintf( stderr,
                 "\nMEMORY LEAK!  %lu un-freed allocations remain\n",
                 outstandingCount );
}

#endif

