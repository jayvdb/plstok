/* plspriv.h -- prototypes for routines for internal use, not to be
   visible to the client code.  Should be preceded by plstok.h.

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

#ifndef PLSPRIV_H
#define PLSPRIV_H

#ifdef __cplusplus
	extern "C" {
#endif

int pls_append_msg( Pls_tok * pT, const char * str );
int pls_append_text( Pls_tok * pT, const char * str );
Pls_token_type pls_keyword( const char * s );
Pls_tok * pls_alloc_tok( void );

#ifdef __cplusplus
	};
#endif

#endif
