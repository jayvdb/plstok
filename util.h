/* util.h -- header for miscellaneous useful functions and macros

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

#ifndef UTIL_H
#define UTIL_H

#define TRUE        (1)
#define FALSE       (0)
#define OKAY        (0)
#define ERROR_FOUND (1)

#ifdef __cplusplus
	extern "C" {
#endif

char * strDup( const char * s );
void * allocMemory( size_t size );
void * allocNulMemory(size_t nitems, size_t size);
void freeMemory( void * pMem );
void * resizeMemory( void * pOld, size_t size );
void registerMemoryPool( void (* pFunction) (void *), void * p );
void unRegisterMemoryPool( void (* pFunction) (void *), const void * p );
void purgeMemoryPools( void );

#ifdef __cplusplus
	};
#endif

/* The following variant of assert() is mostly cribbed from: Writing Solid
   Code, by Steve Maguire, p. 17; Microsoft Press, 1993, Redmond, WA.
*/

#ifndef NDEBUG

#ifdef __cplusplus
	extern "C"
#endif

	void myAssert( const char * sourceFile, unsigned sourceLine );

#define ASSERT(f)             \
		if( f )                   \
			;                     \
		else                      \
			myAssert( __FILE__, __LINE__ )
#else
#define ASSERT(f)
#endif

#endif
