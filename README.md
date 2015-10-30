Free PL/SQL Utilities
---------------------

This project is the home of some free utilities for managing PL/SQL source code by Scott McKellar <mck9@swbell.net>.
The source comes from Wayback Machine: https://web.archive.org/web/20010706235240/http://home.swbell.net/mck9/pls/index.html

The earlier and latter captures on Wayback Machine are identical until the last version archived on 18 August 2007 : https://web.archive.org/web/20070818235915/http://home.swbell.net/mck9/pls/index.html

These utilities are available in the form of C source code, distributed
under the terms of the GNU General Public License.  If you have an ISO C
compiler, and know how to use it, you can download the code and compile it
for your environment.  You can also modify the code to suit your own tastes
and needs.

At the core of these utilities lies 'plstok', a
tokenizer or lexical analyzer for PL/SQL code.  Plstok is more significant
than the utilities themselves, because you can incorporate it into tools of 
your own devising.  Given plstok, most of the utilities below so far were 
trivial to write.  The exception -- and the reason for developing plstok
in the first place -- is plsb, a beautifier for PL/SQL source code.

Oracle 8 uses additional reserved words not recognized by plstok, which was
written for Oracle 7.  In large part you could probably adapt these utilities
for Oracle 8 simply by editing plstok.h to include the new reserved words. 
There may remain some forms of Oracle 8 syntax which are not properly 
handled, especially by plsb.

Utilities Available
~~~~~~~~~~~~~~~~~~~

The tools currently available include:

* ttok -- a demo and test driver for the tokenizer, plstok.

* plscap -- raises reserved words to upper case,
and changes identifiers to lower case.

* plscount -- counts PL/SQL tokens.

* plsenull -- finds constructs which attempt
to test for equality (or inequality) to NULL.

* plsqlf -- finds string or character
literals which contain embedded line feeds.

* plsb -- reformats PL/SQL source code to apply
consistent use of indentation and white space (still under development).


On the Use of Text Files
~~~~~~~~~~~~~~~~~~~~~~~~
The PL/SQL utilities presented here treat PL/SQL source code as text files.

In real life, of course, Oracle stores the code in the database.  You're
expected to edit it with Procedure Builder or some such tool.  Oracle doesn't
make it easy to access the code with text-based tools like sed or grep,
no matter how useful it would be to do so.

So why am I sticking with the text-based approach?

* To keep things simple.  I can concentrate on the problems of lexical
and semantic analysis without being distracted by embedded SQL and GUI
interfaces.

* To keep things portable.  I'm not confining myself to a particular
environment.

* To keep things free.  I don't have Oracle at home in the basement.  To
develop tools which access the database I would have to use my employer's 
system, and then I wouldn't legally be able to release the source code
as open software.


On my job, I developed a suite of utilities for fetching and compiling 
PL/SQL modules from the Unix command line.  (I can't release these utilities
because I wrote them on company time; they are the intellectual property of 
my employer.)  As a result I can manage our PL/SQL as a collection of text 
files.  Using grep I can quickly find, for example, all calls to 
such-and-such a procedure.

Hence the utilities offered here are a good fit for my own environment.
If your environment is not text-based like mine is, there are three ways
to use these utilities:


* Convert your PL/SQL code in and out of text format as needed.  For
example, use the export and import facilities of Procedure Builder.  This
approach will be cumbersome for large numbers of files.

* Write your own utilities for fetching and compiling PL/SQL source
code from the database, as I did.

* Adapt my source code for your own environment.  Plstok lets you install
a callback function to read source code from something other than a text 
file -- such a database.


Compiling
~~~~~~~~~

Compile each of the C files in the 'plstok' package
and whatever C files are specific for a given utility.  Link them.  That's
it.

How you do that depends on your system.  Since there's nothing very 
complicated about the builds, I haven't tried to provide a makefile (which 
would likely need to be customized for your system anyway).

However, there is one small complication.  By default, the code includes
a lot of debugging assertions.  In addition, the memory management layer
displays a brief report of memory usage at the end of a run.

To disable the debugging code and the memory usage report, compile all
modules with the macro NDEBUG #defined.  Compilers generally provide a way
to #define a macro without changing the source code.  In Unix, for example,
use the -DNDEBUG option.

If you compile with NDEBUG #defined, then you needn't include the myassert
module in the link.

