/* myAssert -- called by ASSERT macro.  This code is largely based on
   code published in: Writing Solid Code, by Steve Maguire, p. 18;
   Microsoft Press, 1993, Redmond, WA.

   Consequently the usual boilerplate about the GNU General Public
   License does not apply to this file.  Besides, the code is so trivial
   that it neither needs nor merits license protection.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

#include <stdlib.h>
#include <stdio.h>
#include "util.h"

void myAssert( const char * sourceFile, unsigned sourceLine )
{
	fflush( stdout );
	fprintf( stderr, "\nAssertion failed: %s, line %u\n",
			 sourceFile, sourceLine );
	fflush( stderr );
	abort();
}

