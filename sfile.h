/* sfile.h: header for Sfile functions.  An Sfile encapsulates source code
   to be parsed, interpreted, or compiled.

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

#ifndef SFILE_H
#define SFILE_H

/* typedef for a function ptr for a callback function to
   fetch a character from some source */

#ifdef __cplusplus
	typedef int (* "C" SF_func)( void * p );
#else
	typedef int (* SF_func)( void * p );
#endif

typedef struct
{
	void * p;	/* opaque pointer to internal structure */
} Sfile;

/* struct for representing the position within a source */

typedef struct
{
	int line;
	int col;
} Sposition;

#ifdef __cplusplus
	extern "C" {
#endif

Sfile s_open( const char * filename );
Sfile s_assign( FILE * pF );
Sfile s_callback( SF_func func, void * p );
int s_getc( Sfile S );
int s_ungetc( Sfile s, int c );
Sposition s_position( Sfile s );
void s_close( Sfile * pS );

#ifdef __cplusplus
	};
#endif

#endif
