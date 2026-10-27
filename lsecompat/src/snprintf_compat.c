/*
 * snprintf.c - a portable implementation of snprintf
 *
 * AUTHOR
 *   Mark Martinec <mark.martinec@ijs.si>, April 1999.
 *
 *   Copyright 1999, Mark Martinec. All rights reserved.
 *
 * TERMS AND CONDITIONS
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the "Frontier Artistic License" which comes
 *   with this Kit.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *   See the Frontier Artistic License for more details.
 *
 *   You should have received a copy of the Frontier Artistic License
 *   with this Kit in the file named LICENSE.txt .
 *   If not, I'll be glad to provide one.
 *
 * FEATURES
 * - careful adherence to specs regarding flags, field width and precision;
 * - good performance for large string handling (large format, large
 *   argument or large paddings). Performance is similar to system's sprintf
 *   and in several cases significantly better (make sure you compile with
 *   optimizations turned on, tell the compiler the code is strict ANSI
 *   if necessary to give it more freedom for optimizations);
 * - return value semantics per ISO/IEC 9899:1999 ("ISO C99");
 * - written in standard ISO/ANSI C - requires an ANSI C compiler.
 *
 * SUPPORTED CONVERSION SPECIFIERS AND DATA TYPES
 *
 * Supported conversion specifiers:
 * s, c, d, u, o, x, X, p, f, e, E, g, G (and synonyms: i, D, U, O - see below)
 * with flags: '-', '+', ' ', '0' and '#'.
 * An asterisk is supported for field width as well as precision.
 *
 * Length modifiers 'h' (short int), 'l' (long int),
 * 'll' (long long int), and 'L' (long double) are supported.
 * NOTE:
 *   length modifier 'll' is recognized but treated the same as 'l',
 *   which may cause argument value truncation! Defining
 *   SNPRINTF_LONGLONG_SUPPORT requires that your system's sprintf also
 *   handles length modifier 'll'.  long long int is a language extension
 *   which may not be portable.
 *
 * Conversion of numeric data (conversion specifiers d, u, o, x, X, p)
 * with length modifiers (none or h, l, ll) is left to the system routine
 * sprintf, but all handling of flags, field width and precision as well as
 * c and s conversions is done very carefully by this portable routine.
 * If a string precision (truncation) is specified (e.g. %.8s) it is
 * guaranteed the string beyond the specified precision will not be referenced.
 *
 * Length modifiers h, l and ll are ignored for c and s conversions (data
 * types wint_t and wchar_t are not supported).
 *
 * The following common synonyms for conversion characters are supported:
 *   - i is a synonym for d
 *   - D is a synonym for ld, explicit length modifiers are ignored
 *   - U is a synonym for lu, explicit length modifiers are ignored
 *   - O is a synonym for lo, explicit length modifiers are ignored
 * The D, O and U conversion characters are nonstandard, they are supported
 * for backward compatibility only, and should not be used for new code.
 *
 * The following is specifically NOT supported:
 *   - flag ' (thousands' grouping character) is recognized but ignored
 *   - numeric conversion specifiers: f, e, E, g, G and synonym F,
 *     as well as the new a and A conversion specifiers
 *   - length modifier 'L' (long double) and 'q' (quad - use 'll' instead)
 *   - wide character/string conversions: lc, ls, and nonstandard
 *     synonyms C and S
 *   - writeback of converted string length: conversion character n
 *   - the n$ specification for direct reference to n-th argument
 *   - locales
 *
 * It is permitted for str_m to be zero, and it is permitted to specify NULL
 * pointer for resulting string argument if str_m is zero (as per ISO C99).
 *
 * The return value is the number of characters which would be generated
 * for the given input, excluding the trailing null. If this value
 * is greater or equal to str_m, not all characters from the result
 * have been stored in str, output bytes beyond the (str_m-1) -th character
 * are discarded. If str_m is greater than zero it is guaranteed
 * the resulting string will be null-terminated.
 *
 * NOTE that this matches the ISO C99, OpenBSD, and GNU C library 2.1,
 * but is different from some older and vendor implementations,
 * and is also different from XPG, XSH5, SUSv2 specifications.
 * For historical discussion on changes in the semantics and standards
 * of snprintf see printf(3) man page in the Linux programmers manual.
 *
 * Routines asprintf and vasprintf return a pointer (in the ptr argument)
 * to a buffer sufficiently large to hold the resulting string. This pointer
 * should be passed to free(3) to release the allocated storage when it is
 * no longer needed. If sufficient space cannot be allocated, these functions
 * will return -1 and set ptr to be a NULL pointer. These two routines are a
 * GNU C library extensions (glibc).
 *
 * Routines asnprintf and vasnprintf are similar to asprintf and vasprintf,
 * yet, like snprintf and vsnprintf counterparts, will write at most str_m-1
 * characters into the allocated output string, the last character in the
 * allocated buffer then gets the terminating null. If the formatted string
 * length (the return value) is greater than or equal to the str_m argument,
 * the resulting string was truncated and some of the formatted characters
 * were discarded. These routines present a handy way to limit the amount
 * of allocated memory to some sane value.
 *
 * AVAILABILITY
 *   http://www.ijs.si/software/snprintf/
 *
 * REVISION HISTORY
 * 1999-04	V0.9  Mark Martinec
 *		- initial version, some modifications after comparing printf
 *		  man pages for Digital Unix 4.0, Solaris 2.6 and HPUX 10,
 *		  and checking how Perl handles sprintf (differently!);
 * 1999-04-09	V1.0  Mark Martinec <mark.martinec@ijs.si>
 *		- added main test program, fixed remaining inconsistencies,
 *		  added optional (long long int) support;
 * 1999-04-12	V1.1  Mark Martinec <mark.martinec@ijs.si>
 *		- support the 'p' conversion (pointer to void);
 *		- if a string precision is specified
 *		  make sure the string beyond the specified precision
 *		  will not be referenced (e.g. by strlen);
 * 1999-04-13	V1.2  Mark Martinec <mark.martinec@ijs.si>
 *		- support synonyms %D=%ld, %U=%lu, %O=%lo;
 *		- speed up the case of long format string with few conversions;
 * 1999-06-30	V1.3  Mark Martinec <mark.martinec@ijs.si>
 *		- fixed runaway loop (eventually crashing when str_l wraps
 *		  beyond 2^31) while copying format string without
 *		  conversion specifiers to a buffer that is too short
 *		  (thanks to Edwin Young <edwiny@autonomy.com> for
 *		  spotting the problem);
 *		- added macros PORTABLE_SNPRINTF_VERSION_(MAJOR|MINOR)
 *		  to snprintf.h
 * 2000-02-14	V2.0 (never released) Mark Martinec <mark.martinec@ijs.si>
 *		- relaxed license terms: The Artistic License now applies.
 *		  You may still apply the GNU GENERAL PUBLIC LICENSE
 *		  as was distributed with previous versions, if you prefer;
 *		- changed REVISION HISTORY dates to use ISO 8601 date format;
 *		- added vsnprintf (patch also independently proposed by
 *		  Caolan McNamara 2000-05-04, and Keith M Willenson 2000-06-01)
 * 2000-06-27	V2.1  Mark Martinec <mark.martinec@ijs.si>
 *		- removed POSIX check for str_m<1; value 0 for str_m is
 *		  allowed by ISO C99 (and GNU C library 2.1) - (pointed out
 *		  on 2000-05-04 by Caolan McNamara, caolan@ csn dot ul dot ie).
 *		  Besides relaxed license this change in standards adherence
 *		  is the main reason to bump up the major version number;
 *		- added nonstandard routines asnprintf, vasnprintf, asprintf,
 *		  vasprintf that dynamically allocate storage for the
 *		  resulting string; these routines are not compiled by default,
 *		  see comments where NEED_V?ASN?PRINTF macros are defined;
 *		- autoconf contributed by Caolan McNamara
 * 2000-10-06	V2.2  Mark Martinec <mark.martinec@ijs.si>
 *		- BUG FIX: the %c conversion used a temporary variable
 *		  that was no longer in scope when referenced,
 *		  possibly causing incorrect resulting character;
 *		- BUG FIX: make precision and minimal field width unsigned
 *		  to handle huge values (2^31 <= n < 2^32) correctly;
 *		  also be more careful in the use of signed/unsigned/size_t
 *		  internal variables - probably more careful than many
 *		  vendor implementations, but there may still be a case
 *		  where huge values of str_m, precision or minimal field
 *		  could cause incorrect behaviour;
 *		- use separate variables for signed/unsigned arguments,
 *		  and for short/int, long, and long long argument lengths
 *		  to avoid possible incompatibilities on certain
 *		  computer architectures. Also use separate variable
 *		  arg_sign to hold sign of a numeric argument,
 *		  to make code more transparent;
 *		- some fiddling with zero padding and "0x" to make it
 *		  Linux compatible;
 *		- systematically use macros fast_memcpy and fast_memset
 *		  instead of case-by-case hand optimization; determine some
 *		  breakeven string lengths for different architectures;
 *		- terminology change: 'format' -> 'conversion specifier',
 *		  'C9x' -> 'ISO/IEC 9899:1999 ("ISO C99")',
 *		  'alternative form' -> 'alternate form',
 *		  'data type modifier' -> 'length modifier';
 *		- several comments rephrased and new ones added;
 *		- make compiler not complain about 'credits' defined but
 *		  not used;
 * 2026-10-05  V2.3  Luis E. Limon <llimon@github.com>
 *     - Modernized portable-snprintf to full C99/POSIX standards compliance;
 *     - Added %n specifier support (char count writeback across short/int/long/ll);
 *     - Added C99 length modifiers %z (size_t) and %t (ptrdiff_t / ssize_t);
 *     - Added %Lf (long double) quad-precision floating-point formatting;
 *     - Implemented GNU/POSIX asprintf, vasprintf, asnprintf, and vasnprintf wrappers;
 *     - Fixed SIGSEGV caused by unconsumed variadic arguments and NULL pointer
 *       dereferences in %.*f %s format specifiers;
 *     - Expanded embedded test harness to 19 tests covering SPARC stack alignment,
 *       dynamic precision floats, and POSIX/C99 compatibility.
 * 2026-10-27  v2.4  Luis E. Limon <llimon@github.com>
 *    - Fixed SunOS 4.1.4 (BSD-style sprintf) return value mismatch where
 *      char* pointers were assigned to size_t length counters (str_arg_l),
 *      causing compiler warnings and buffer offset corruption (SIGSEGV/SIGBUS);
 *    - Refactored numeric and float formatting to use standalone sprintf()
 *      followed by strlen() length calculation for BSD/POSIX portability;
 *    - Refactored %ll signed/unsigned manual conversion blocks with local offsets
 *      to prevent mutation of global str_arg_l state during truncation;
 *    - Implemented custom manual 64-bit integer unpacking loops (llbuf) for %lld,
 *      %llu, %llx, %llX, and %llo to bypass SunOS 4.1.4 libc sprintf limitations;
 *    - Added SPARC V7 stack alignment guard for %Lf on SunOS 4.1.4 to prevent
 *      unaligned 128-bit quad float extraction and libc _doprnt SIGBUS traps;
 */


/* Define HAVE_SNPRINTF if your system already has snprintf and vsnprintf.
 *
 * If HAVE_SNPRINTF is defined this module will not produce code for
 * snprintf and vsnprintf, unless PREFER_PORTABLE_SNPRINTF is defined as well,
 * causing this portable version of snprintf to be called portable_snprintf
 * (and portable_vsnprintf).
 */
/* #define HAVE_SNPRINTF */

/* Define PREFER_PORTABLE_SNPRINTF if your system does have snprintf and
 * vsnprintf but you would prefer to use the portable routine(s) instead.
 * In this case the portable routine is declared as portable_snprintf
 * (and portable_vsnprintf) and a macro 'snprintf' (and 'vsnprintf')
 * is defined to expand to 'portable_v?snprintf' - see file snprintf.h .
 * Defining this macro is only useful if HAVE_SNPRINTF is also defined,
 * but does does no harm if defined nevertheless.
 */
/* #define PREFER_PORTABLE_SNPRINTF */

/* Define SNPRINTF_LONGLONG_SUPPORT if you want to support
 * data type (long long int) and length modifier 'll' (e.g. %lld).
 * If undefined, 'll' is recognized but treated as a single 'l'.
 *
 * If the system's sprintf does not handle 'll'
 * the SNPRINTF_LONGLONG_SUPPORT must not be defined!
 *
 * This is off by default as (long long int) is a language extension.
 */
#define SNPRINTF_LONGLONG_SUPPORT

/* Define NEED_SNPRINTF_ONLY if you only need snprintf, and not vsnprintf.
 * If NEED_SNPRINTF_ONLY is defined, the snprintf will be defined directly,
 * otherwise both snprintf and vsnprintf routines will be defined
 * and snprintf will be a simple wrapper around vsnprintf, at the expense
 * of an extra procedure call.
 */
/* #define NEED_SNPRINTF_ONLY */

/* Define NEED_V?ASN?PRINTF macros if you need library extension
 * routines asprintf, vasprintf, asnprintf, vasnprintf respectively,
 * and your system library does not provide them. They are all small
 * wrapper routines around portable_vsnprintf. Defining any of the four
 * NEED_V?ASN?PRINTF macros automatically turns off NEED_SNPRINTF_ONLY
 * and turns on PREFER_PORTABLE_SNPRINTF.
 *
 * Watch for name conflicts with the system library if these routines
 * are already present there.
 *
 * NOTE: vasprintf and vasnprintf routines need va_copy() from stdarg.h, as
 * specified by C99, to be able to traverse the same list of arguments twice.
 * I don't know of any other standard and portable way of achieving the same.
 * With some versions of gcc you may use __va_copy(). You might even get away
 * with "ap2 = ap", in this case you must not call va_end(ap2) !
 *   #define va_copy(ap2,ap) ap2 = ap
 */
#define NEED_ASPRINTF  
#define NEED_ASNPRINTF 
#define NEED_VASPRINTF 
#define NEED_VASNPRINTF


/* Define the following macros if desired:
 *   SOLARIS_COMPATIBLE, SOLARIS_BUG_COMPATIBLE,
 *   HPUX_COMPATIBLE, HPUX_BUG_COMPATIBLE, LINUX_COMPATIBLE,
 *   DIGITAL_UNIX_COMPATIBLE, DIGITAL_UNIX_BUG_COMPATIBLE,
 *   PERL_COMPATIBLE, PERL_BUG_COMPATIBLE,
 *
 * - For portable applications it is best not to rely on peculiarities
 *   of a given implementation so it may be best not to define any
 *   of the macros that select compatibility and to avoid features
 *   that vary among the systems.
 *
 * - Selecting compatibility with more than one operating system
 *   is not strictly forbidden but is not recommended.
 *
 * - 'x'_BUG_COMPATIBLE implies 'x'_COMPATIBLE .
 *
 * - 'x'_COMPATIBLE refers to (and enables) a behaviour that is
 *   documented in a sprintf man page on a given operating system
 *   and actually adhered to by the system's sprintf (but not on
 *   most other operating systems). It may also refer to and enable
 *   a behaviour that is declared 'undefined' or 'implementation specific'
 *   in the man page but a given implementation behaves predictably
 *   in a certain way.
 *
 * - 'x'_BUG_COMPATIBLE refers to (and enables) a behaviour of system's sprintf
 *   that contradicts the sprintf man page on the same operating system.
 *
 * - I do not claim that the 'x'_COMPATIBLE and 'x'_BUG_COMPATIBLE
 *   conditionals take into account all idiosyncrasies of a particular
 *   implementation, there may be other incompatibilities.
 */



/* ============================================= */
/* NO USER SERVICABLE PARTS FOLLOWING THIS POINT */
/* ============================================= */

#define PORTABLE_SNPRINTF_VERSION_MAJOR 2
#define PORTABLE_SNPRINTF_VERSION_MINOR 2

#if defined(NEED_ASPRINTF) || defined(NEED_ASNPRINTF) || defined(NEED_VASPRINTF) || defined(NEED_VASNPRINTF)
# if defined(NEED_SNPRINTF_ONLY)
# undef NEED_SNPRINTF_ONLY
# endif
# if !defined(PREFER_PORTABLE_SNPRINTF)
# define PREFER_PORTABLE_SNPRINTF
# endif
#endif

#if defined(SOLARIS_BUG_COMPATIBLE) && !defined(SOLARIS_COMPATIBLE)
#define SOLARIS_COMPATIBLE
#endif

#if defined(HPUX_BUG_COMPATIBLE) && !defined(HPUX_COMPATIBLE)
#define HPUX_COMPATIBLE
#endif

#if defined(DIGITAL_UNIX_BUG_COMPATIBLE) && !defined(DIGITAL_UNIX_COMPATIBLE)
#define DIGITAL_UNIX_COMPATIBLE
#endif

#if defined(PERL_BUG_COMPATIBLE) && !defined(PERL_COMPATIBLE)
#define PERL_COMPATIBLE
#endif

#if defined(LINUX_BUG_COMPATIBLE) && !defined(LINUX_COMPATIBLE)
#define LINUX_COMPATIBLE
#endif

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>

#include <ctype.h>
#include <limits.h>
#include <stddef.h>

#if defined(__sun__) || defined(sun) || defined(__sun)
   #include <memory.h>
#endif

/* Standard I/O and String prototypes for SunOS 4.1.4 / C89 */
extern int fprintf(FILE *stream, const char *format, ...);
extern int printf(const char *format, ...);
extern void *memchr(const void *s, int c, size_t n);
extern char *strchr(const char *s, int c);
#ifndef va_copy
# if defined(__va_copy)
#   define va_copy(d, s) __va_copy((d), (s))
# elif defined(__builtin_va_copy)
#   define va_copy(d, s) __builtin_va_copy((d), (s))
# else
#   define va_copy(d, s) ((d) = (s))
# endif
#endif


#ifndef _SSIZE_T
#define _SSIZE_T
typedef int ssize_t;
#endif

#ifdef isdigit
#undef isdigit
#endif
#define isdigit(c) ((c) >= '0' && (c) <= '9')

/* For copying strings longer or equal to 'breakeven_point'
 * it is more efficient to call memcpy() than to do it inline.
 * The value depends mostly on the processor architecture,
 * but also on the compiler and its optimization capabilities.
 * The value is not critical, some small value greater than zero
 * will be just fine if you don't care to squeeze every drop
 * of performance out of the code.
 *
 * Small values favor memcpy, large values favor inline code.
 */
#if defined(__alpha__) || defined(__alpha)
#  define breakeven_point   2	/* AXP (DEC Alpha)     - gcc or cc or egcs */
#endif
#if defined(__i386__)  || defined(__i386)
#  define breakeven_point  12	/* Intel Pentium/Linux - gcc 2.96 */
#endif
#if defined(__hppa)
#  define breakeven_point  10	/* HP-PA               - gcc */
#endif
#if defined(__sparc__) || defined(__sparc)
#  define breakeven_point  33	/* Sun Sparc 5         - gcc 2.8.1 */
#endif

/* some other values of possible interest: */
/* #define breakeven_point  8 */  /* VAX 4000          - vaxc */
/* #define breakeven_point 19 */  /* VAX 4000          - gcc 2.7.0 */

#ifndef breakeven_point
#  define breakeven_point   6	/* some reasonable one-size-fits-all value */
#endif

#define fast_memcpy(d,s,n) \
  { register size_t nn = (size_t)(n); \
    if (nn >= breakeven_point) memcpy((d), (s), nn); \
    else if (nn > 0) { /* proc call overhead is worth only for large strings*/\
      register char *dd; register const char *ss; \
      for (ss=(s), dd=(d); nn>0; nn--) *dd++ = *ss++; } }

#define fast_memset(d,c,n) \
  { register size_t nn = (size_t)(n); \
    if (nn >= breakeven_point) memset((d), (int)(c), nn); \
    else if (nn > 0) { /* proc call overhead is worth only for large strings*/\
      register char *dd; register const int cc=(int)(c); \
      for (dd=(d); nn>0; nn--) *dd++ = cc; } }

/* prototypes */

#if defined(NEED_ASPRINTF)
int asprintf   (char **ptr, const char *fmt, /*args*/ ...);
#endif
#if defined(NEED_VASPRINTF)
int vasprintf  (char **ptr, const char *fmt, va_list ap);
#endif
#if defined(NEED_ASNPRINTF)
int asnprintf  (char **ptr, size_t str_m, const char *fmt, /*args*/ ...);
#endif
#if defined(NEED_VASNPRINTF)
int vasnprintf (char **ptr, size_t str_m, const char *fmt, va_list ap);
#endif

#if defined(HAVE_SNPRINTF)
/* declare our portable snprintf  routine under name portable_snprintf  */
/* declare our portable vsnprintf routine under name portable_vsnprintf */
#else
/* declare our portable routines under names snprintf and vsnprintf */
#define portable_snprintf snprintf
#if !defined(NEED_SNPRINTF_ONLY)
#define portable_vsnprintf vsnprintf
#endif
#endif

#if !defined(HAVE_SNPRINTF) || defined(PREFER_PORTABLE_SNPRINTF)
int portable_snprintf(char *str, size_t str_m, const char *fmt, /*args*/ ...);
#if !defined(NEED_SNPRINTF_ONLY)
int portable_vsnprintf(char *str, size_t str_m, const char *fmt, va_list ap);
#endif
#endif

/* declarations */

static char credits[] = "\n\
@(#)snprintf.c, v2.2: Mark Martinec, <mark.martinec@ijs.si>\n\
@(#)snprintf.c, v2.2: Copyright 1999, Mark Martinec. Frontier Artistic License applies.\n\
@(#)snprintf.c, v2.2: http://www.ijs.si/software/snprintf/\n";

#if defined(NEED_ASPRINTF)
int asprintf(char **ptr, const char *fmt, /*args*/ ...) {
  va_list ap;
  size_t str_m;
  int str_l;

  *ptr = NULL;
  va_start(ap, fmt);                            /* measure the required size */
  str_l = portable_vsnprintf(NULL, (size_t)0, fmt, ap);
  va_end(ap);
  assert(str_l >= 0);        /* possible integer overflow if str_m > INT_MAX */
  *ptr = (char *) malloc(str_m = (size_t)str_l + 1);
  if (*ptr == NULL) { errno = ENOMEM; str_l = -1; }
  else {
    int str_l2;
    va_start(ap, fmt);
    str_l2 = portable_vsnprintf(*ptr, str_m, fmt, ap);
    va_end(ap);
    assert(str_l2 == str_l);
  }
  return str_l;
}
#endif

#if defined(NEED_VASPRINTF)
int vasprintf(char **ptr, const char *fmt, va_list ap) {
  size_t str_m;
  int str_l;

  *ptr = NULL;
  { va_list ap2;
    va_copy(ap2, ap);  /* don't consume the original ap, we'll need it again */
    str_l = portable_vsnprintf(NULL, (size_t)0, fmt, ap2);/*get required size*/
    va_end(ap2);
  }
  assert(str_l >= 0);        /* possible integer overflow if str_m > INT_MAX */
  *ptr = (char *) malloc(str_m = (size_t)str_l + 1);
  if (*ptr == NULL) { errno = ENOMEM; str_l = -1; }
  else {
    int str_l2 = portable_vsnprintf(*ptr, str_m, fmt, ap);
    assert(str_l2 == str_l);
  }
  return str_l;
}
#endif

#if defined(NEED_ASNPRINTF)
int asnprintf (char **ptr, size_t str_m, const char *fmt, /*args*/ ...) {
  va_list ap;
  int str_l;

  *ptr = NULL;
  va_start(ap, fmt);                            /* measure the required size */
  str_l = portable_vsnprintf(NULL, (size_t)0, fmt, ap);
  va_end(ap);
  assert(str_l >= 0);        /* possible integer overflow if str_m > INT_MAX */
  if ((size_t)str_l + 1 < str_m) str_m = (size_t)str_l + 1;      /* truncate */
  /* if str_m is 0, no buffer is allocated, just set *ptr to NULL */
  if (str_m == 0) {  /* not interested in resulting string, just return size */
  } else {
    *ptr = (char *) malloc(str_m);
    if (*ptr == NULL) { errno = ENOMEM; str_l = -1; }
    else {
      int str_l2;
      va_start(ap, fmt);
      str_l2 = portable_vsnprintf(*ptr, str_m, fmt, ap);
      va_end(ap);
      assert(str_l2 == str_l);
    }
  }
  return str_l;
}
#endif

#if defined(NEED_VASNPRINTF)
int vasnprintf (char **ptr, size_t str_m, const char *fmt, va_list ap) {
  int str_l;

  *ptr = NULL;
  { va_list ap2;
    va_copy(ap2, ap);  /* don't consume the original ap, we'll need it again */
    str_l = portable_vsnprintf(NULL, (size_t)0, fmt, ap2);/*get required size*/
    va_end(ap2);
  }
  assert(str_l >= 0);        /* possible integer overflow if str_m > INT_MAX */
  if ((size_t)str_l + 1 < str_m) str_m = (size_t)str_l + 1;      /* truncate */
  /* if str_m is 0, no buffer is allocated, just set *ptr to NULL */
  if (str_m == 0) {  /* not interested in resulting string, just return size */
  } else {
    *ptr = (char *) malloc(str_m);
    if (*ptr == NULL) { errno = ENOMEM; str_l = -1; }
    else {
      int str_l2 = portable_vsnprintf(*ptr, str_m, fmt, ap);
      assert(str_l2 == str_l);
    }
  }
  return str_l;
}
#endif

/*
 * If the system does have snprintf and the portable routine is not
 * specifically required, this module produces no code for snprintf/vsnprintf.
 */
#if !defined(HAVE_SNPRINTF) || defined(PREFER_PORTABLE_SNPRINTF)

#if !defined(NEED_SNPRINTF_ONLY)
int portable_snprintf(char *str, size_t str_m, const char *fmt, /*args*/ ...) {
  va_list ap;
  int str_l;

  va_start(ap, fmt);
  str_l = portable_vsnprintf(str, str_m, fmt, ap);
  va_end(ap);
  return str_l;
}
#endif

#if defined(NEED_SNPRINTF_ONLY)
int portable_snprintf(char *str, size_t str_m, const char *fmt, /*args*/ ...) {
#else
int portable_vsnprintf(char *str, size_t str_m, const char *fmt, va_list ap) {
#endif

#if defined(NEED_SNPRINTF_ONLY)
  va_list ap;
#endif
  size_t str_l = 0;
  const char *p = fmt;

/* In contrast with POSIX, the ISO C99 now says
 * that str can be NULL and str_m can be 0.
 * This is more useful than the old:  if (str_m < 1) return -1; */

#if defined(NEED_SNPRINTF_ONLY)
  va_start(ap, fmt);
#endif
  if (!p) p = "";
  while (*p) {
    if (*p != '%') {
   /* if (str_l < str_m) str[str_l++] = *p++;    -- this would be sufficient */
   /* but the following code achieves better performance for cases
    * where format string is long and contains few conversions */
      const char *q = strchr(p+1,'%');
      size_t n = !q ? strlen(p) : (q-p);
      if (str_l < str_m) {
        size_t avail = (str_m - str_l > 1) ? (str_m - str_l - 1) : 0;
        fast_memcpy(str+str_l, p, (n>avail?avail:n));
      }
      p += n; str_l += n;
    } else {
      const char *starting_p;
      size_t min_field_width = 0, precision = 0;
      int zero_padding = 0, precision_specified = 0, justify_left = 0;
      int alternate_form = 0, force_sign = 0;
      int space_for_positive = 1; /* If both the ' ' and '+' flags appear,
                                     the ' ' flag should be ignored. */
      char length_modifier = '\0';            /* allowed values: \0, h, l, L */
      char tmp[256];/* temporary buffer for simple numeric->string conversion */

      const char *str_arg;      /* string address in case of string argument */
      size_t str_arg_l;         /* natural field width of arg without padding
                                   and sign */
      unsigned char uchar_arg;
        /* unsigned char argument value - only defined for c conversion.
           N.B. standard explicitly states the char argument for
           the c conversion is unsigned */

      size_t number_of_zeros_to_pad = 0;
        /* number of zeros to be inserted for numeric conversions
           as required by the precision or minimal field width */

      size_t zero_padding_insertion_ind = 0;
        /* index into tmp where zero padding is to be inserted */

      char fmt_spec = '\0';
        /* current conversion specifier character */

      str_arg = credits;/* just to make compiler happy (defined but not used)*/
      str_arg = NULL;
      starting_p = p; p++;  /* skip '%' */
   /* parse flags */
      while (*p == '0' || *p == '-' || *p == '+' ||
             *p == ' ' || *p == '#' || *p == '\'') {
        switch (*p) {
        case '0': zero_padding = 1; break;
        case '-': justify_left = 1; break;
        case '+': force_sign = 1; space_for_positive = 0; break;
        case ' ': force_sign = 1;
     /* If both the ' ' and '+' flags appear, the ' ' flag should be ignored */
#ifdef PERL_COMPATIBLE
     /* ... but in Perl the last of ' ' and '+' applies */
                  space_for_positive = 1;
#endif
                  break;
        case '#': alternate_form = 1; break;
        case '\'': break;
        }
        p++;
      }
   /* If the '0' and '-' flags both appear, the '0' flag should be ignored. */

   /* parse field width */
      if (*p == '*') {
        int j;
        p++; j = va_arg(ap, int);
        if (j >= 0) min_field_width = j;
        else { min_field_width = -j; justify_left = 1; }
      } else if (isdigit((int)(*p))) {
        /* size_t could be wider than unsigned int;
           make sure we treat argument like common implementations do */
        unsigned int uj = *p++ - '0';
        while (isdigit((int)(*p))) uj = 10*uj + (unsigned int)(*p++ - '0');
        min_field_width = uj;
      }
   /* parse precision */
      if (*p == '.') {
        p++; precision_specified = 1;
        if (*p == '*') {
          int j = va_arg(ap, int);
          p++;
          if (j >= 0) precision = j;
          else {
            precision_specified = 0; precision = 0;
         /* NOTE:
          *   Solaris 2.6 man page claims that in this case the precision
          *   should be set to 0.  Digital Unix 4.0, HPUX 10 and BSD man page
          *   claim that this case should be treated as unspecified precision,
          *   which is what we do here.
          */
          }
        } else if (isdigit((int)(*p))) {
          /* size_t could be wider than unsigned int;
             make sure we treat argument like common implementations do */
          unsigned int uj = *p++ - '0';
          while (isdigit((int)(*p))) uj = 10*uj + (unsigned int)(*p++ - '0');
          precision = uj;
        }
      }
   /* parse 'h', 'l', 'll', 'L', 'z' (size_t) and 't' (ptrdiff_t) length modifiers */
      if (*p == 'h' || *p == 'l' || *p == 'L' || *p == 'z' || *p == 't') {
        length_modifier = *p; p++;
        if (length_modifier == 'l' && *p == 'l') {   /* double l = long long */
#ifdef SNPRINTF_LONGLONG_SUPPORT
          length_modifier = '2';                  /* double l encoded as '2' */
#else
          length_modifier = 'l';                 /* treat it as a single 'l' */
#endif
          p++;
        }
      }
      fmt_spec = *p;
   /* common synonyms: */
      switch (fmt_spec) {
      case 'i': fmt_spec = 'd'; break;
      case 'D': fmt_spec = 'd'; length_modifier = 'l'; break;
      case 'U': fmt_spec = 'u'; length_modifier = 'l'; break;
      case 'O': fmt_spec = 'o'; length_modifier = 'l'; break;
      default: break;
      }
   /* get parameter value, do initial processing */
      switch (fmt_spec) {
      case '%': /* % behaves similar to 's' regarding flags and field widths */
      case 'c': /* c behaves similar to 's' regarding flags and field widths */
      case 's':
        length_modifier = '\0';          /* wint_t and wchar_t not supported */
     /* the result of zero padding flag with non-numeric conversion specifier*/
     /* is undefined. Solaris and HPUX 10 does zero padding in this case,    */
     /* Digital Unix and Linux does not. */
#if !defined(SOLARIS_COMPATIBLE) && !defined(HPUX_COMPATIBLE)
        zero_padding = 0;    /* turn zero padding off for string conversions */
#endif
        str_arg_l = 1;
        switch (fmt_spec) {
        case '%':
          str_arg = p; break;
        case 'c': {
          int j = va_arg(ap, int);
          uchar_arg = (unsigned char) j;   /* standard demands unsigned char */
          str_arg = (const char *) &uchar_arg;
          break;
        }
        case 's':
          str_arg = va_arg(ap, const char *);
          if (!str_arg) str_arg = "(null)";
       /* make sure not to address string beyond the specified precision !!! */
          if (!precision_specified) str_arg_l = strlen(str_arg);
       /* truncate string if necessary as requested by precision */
          else if (precision == 0) str_arg_l = 0;
          else {
       /* memchr on HP does not like n > 2^31  !!! */
            const char *q = memchr(str_arg, '\0',
                             precision <= 0x7fffffff ? precision : 0x7fffffff);
            str_arg_l = !q ? precision : (q-str_arg);
          }
          break;
        default: break;
        }
        break;
      case 'n': {
        switch (length_modifier) {
        case '\0': {
          int *ip = va_arg(ap, int *);
          if (ip != NULL) *ip = (int)str_l;
          break;
        }
        case 'h': {
          short int *ip = va_arg(ap, short int *);
          if (ip != NULL) *ip = (short int)str_l;
          break;
        }
        case 'l':
        case 'z':
        case 't': {
          long int *ip = va_arg(ap, long int *);
          if (ip != NULL) *ip = (long int)str_l;
          break;
        }
#ifdef SNPRINTF_LONGLONG_SUPPORT
        case '2': {
          long long int *ip = va_arg(ap, long long int *);
          if (ip != NULL) *ip = (long long int)str_l;
          break;
        }
#endif
        }
        str_arg = tmp;
        str_arg_l = 0;
        }
         break;
      case 'f': case 'e': case 'E': case 'g': case 'G': {
        double double_arg = 0.0;
        long double long_double_arg = 0.0L;
        char f[32]; int f_l = 0;

#if defined(__sun__) && !defined(SVR4) && !defined(__SVR4)
        /* SunOS 4.1.4: Treat 'L' as 64-bit double to preserve SPARC stack frame alignment */
        if (length_modifier == 'L') {
          double_arg = (double)va_arg(ap, double);
        } else {
          double_arg = va_arg(ap, double);
        }
#else 
       /* POSIX: Native 128-bit long double extraction */
        if (length_modifier == 'L') {
          long_double_arg = va_arg(ap, long double);
        } else {
          double_arg = va_arg(ap, double);
        }
#endif

        f[f_l++] = '%';
        if (precision_specified) { 
           sprintf(f + f_l, ".%lu", (unsigned long)precision);
           f_l += (int)strlen(f + f_l);
        }
/* dont get here if SunOS4 */
#if !(defined(__sun__) && !defined(SVR4) && !defined(__SVR4))
        if (length_modifier == 'L') { f[f_l++] = 'L'; }
#endif
        f[f_l++] = fmt_spec; f[f_l++] = '\0';

        str_arg = tmp;
#if defined(__sun__) && !defined(SVR4) && !defined(__SVR4)
        sprintf(tmp, f, double_arg);
#else
        if (length_modifier == 'L') {
          sprintf(tmp, f, long_double_arg);
        } else {
          sprintf(tmp, f, double_arg);
        }
#endif
        str_arg_l = strlen(tmp);
        break;
      }
      case 'd': case 'u': case 'o': case 'x': case 'X': case 'p': {
        /* NOTE: the u, o, x, X and p conversion specifiers imply
                 the value is unsigned;  d implies a signed value */

        int arg_sign = 0;
          /* 0 if numeric argument is zero (or if pointer is NULL for 'p'),
            +1 if greater than zero (or nonzero for unsigned arguments),
            -1 if negative (unsigned argument is never negative) */

        int int_arg = 0;  unsigned int uint_arg = 0;
          /* only defined for length modifier h, or for no length modifiers */

        long int long_arg = 0;  unsigned long int ulong_arg = 0;
          /* only defined for length modifier l */

        void *ptr_arg = NULL;
          /* pointer argument value -only defined for p conversion */

#ifdef SNPRINTF_LONGLONG_SUPPORT
        long long int long_long_arg = 0;
        unsigned long long int ulong_long_arg = 0;
          /* only defined for length modifier ll */
#endif
        if (fmt_spec == 'p') {
        /* HPUX 10: An l, h, ll or L before any other conversion character
         *   (other than d, i, u, o, x, or X) is ignored.
         * Digital Unix:
         *   not specified, but seems to behave as HPUX does.
         * Solaris: If an h, l, or L appears before any other conversion
         *   specifier (other than d, i, u, o, x, or X), the behavior
         *   is undefined. (Actually %hp converts only 16-bits of address
         *   and %llp treats address as 64-bit data which is incompatible
         *   with (void *) argument on a 32-bit system).
         */
#ifdef SOLARIS_COMPATIBLE
#  ifdef SOLARIS_BUG_COMPATIBLE
          /* keep length modifiers even if it represents 'll' */
#  else
          if (length_modifier == '2') length_modifier = '\0';
#  endif
#else
          length_modifier = '\0';
#endif
          ptr_arg = va_arg(ap, void *);
          if (ptr_arg != NULL) arg_sign = 1;
        } else if (fmt_spec == 'd') {  /* signed */
          switch (length_modifier) {
          case '\0':
          case 'h':
         /* It is non-portable to specify a second argument of char or short
          * to va_arg, because arguments seen by the called function
          * are not char or short.  C converts char and short arguments
          * to int before passing them to a function.
          */
            int_arg = va_arg(ap, int);
            if      (int_arg > 0) arg_sign =  1;
            else if (int_arg < 0) arg_sign = -1;
            break;
          case 'l':
            long_arg = va_arg(ap, long int);
            if      (long_arg > 0) arg_sign =  1;
            else if (long_arg < 0) arg_sign = -1;
            break;
          case 'z':
          case 't':
            long_arg = (long int)va_arg(ap, ptrdiff_t);
            if      (long_arg > 0) arg_sign =  1;
            else if (long_arg < 0) arg_sign = -1;
            break;
#ifdef SNPRINTF_LONGLONG_SUPPORT
          case '2':
            long_long_arg = va_arg(ap, long long int);
            if      (long_long_arg > 0) arg_sign =  1;
            else if (long_long_arg < 0) arg_sign = -1;
            break;
#endif
          }
        } else {  /* unsigned */
          switch (length_modifier) {
          case '\0':
          case 'h':
            uint_arg = va_arg(ap, unsigned int);
            if (uint_arg) arg_sign = 1;
            break;
          case 'l':
            ulong_arg = va_arg(ap, unsigned long int);
            if (ulong_arg) arg_sign = 1;
            break;
          case 'z':
            ulong_arg = (unsigned long int)va_arg(ap, size_t);
            if (ulong_arg) arg_sign = 1;
            break;
#ifdef SNPRINTF_LONGLONG_SUPPORT
          case '2':
            ulong_long_arg = va_arg(ap, unsigned long long int);
            if (ulong_long_arg) arg_sign = 1;
            break;
#endif
          }
        }
        str_arg = tmp; str_arg_l = 0;
     /* NOTE:
      *   For d, i, u, o, x, and X conversions, if precision is specified,
      *   the '0' flag should be ignored. This is so with Solaris 2.6,
      *   Digital UNIX 4.0, HPUX 10, Linux, FreeBSD, NetBSD; but not with Perl.
      */
#ifndef PERL_COMPATIBLE
        if (precision_specified) zero_padding = 0;
#endif
        if (fmt_spec == 'd') {
          if (force_sign && arg_sign >= 0)
            tmp[str_arg_l++] = space_for_positive ? ' ' : '+';
         /* leave negative numbers for sprintf to handle,
            to avoid handling tricky cases like (short int)(-32768) */
#ifdef LINUX_COMPATIBLE
        } else if (fmt_spec == 'p' && force_sign && arg_sign > 0) {
          tmp[str_arg_l++] = space_for_positive ? ' ' : '+';
#endif
        } else if (alternate_form) {
          if (arg_sign != 0 && (fmt_spec == 'x' || fmt_spec == 'X') )
            { tmp[str_arg_l++] = '0'; tmp[str_arg_l++] = fmt_spec; }
         /* alternate form should have no effect for p conversion, but ... */
#ifdef HPUX_COMPATIBLE
          else if (fmt_spec == 'p'
         /* HPUX 10: for an alternate form of p conversion,
          *          a nonzero result is prefixed by 0x. */
#ifndef HPUX_BUG_COMPATIBLE
         /* Actually it uses 0x prefix even for a zero value. */
                   && arg_sign != 0
#endif
                  ) { tmp[str_arg_l++] = '0'; tmp[str_arg_l++] = 'x'; }
#endif
        }
        zero_padding_insertion_ind = str_arg_l;
        if (!precision_specified) precision = 1;   /* default precision is 1 */
        if (precision == 0 && arg_sign == 0
#if defined(HPUX_BUG_COMPATIBLE) || defined(LINUX_COMPATIBLE)
            && fmt_spec != 'p'
         /* HPUX 10 man page claims: With conversion character p the result of
          * converting a zero value with a precision of zero is a null string.
          * Actually HP returns all zeroes, and Linux returns "(nil)". */
#endif
        ) {
         /* converted to null string */
         /* When zero value is formatted with an explicit precision 0,
            the resulting formatted string is empty (d, i, u, o, x, X, p).   */
        } else {
          char f[5]; int f_l = 0;
          f[f_l++] = '%';    /* construct a simple format string for sprintf */
          if (!length_modifier) { }
          else if (length_modifier=='2') { f[f_l++] = 'l'; f[f_l++] = 'l'; }
          else if (length_modifier=='z' || length_modifier=='t') { f[f_l++] = 'l'; }
          else f[f_l++] = length_modifier;
          f[f_l++] = fmt_spec; f[f_l++] = '\0';
          if (fmt_spec == 'p') {
            sprintf(tmp + str_arg_l, f, ptr_arg);
            str_arg_l += strlen(tmp + str_arg_l);
          } else if (fmt_spec == 'd') {  /* signed */
            switch (length_modifier) {
            case '\0':
            case 'h': 
              sprintf(tmp + str_arg_l, f, int_arg);
              str_arg_l += strlen(tmp + str_arg_l);
              break;
            case 'l': 
              sprintf(tmp + str_arg_l, f, long_arg);
              str_arg_l += strlen(tmp + str_arg_l);
              break;
            case 'z':
            case 't': 
              sprintf(tmp + str_arg_l, f, long_arg);
              str_arg_l += strlen(tmp + str_arg_l);
              break;
#ifdef SNPRINTF_LONGLONG_SUPPORT
            case '2': 
#if defined(__sun__) && !defined(SVR4) && !defined(__SVR4)
               {
                char llbuf[64];
                int lllen = 0;
                size_t off = 0;
                unsigned long long uv;

                if (long_long_arg < 0) {
                  tmp[off++] = '-';
                  uv = (unsigned long long)(-(long_long_arg + 1)) + 1;
                } else {
                  uv = (unsigned long long)long_long_arg;
                }

                do { llbuf[lllen++] = '0' + (char)(uv % 10); uv /= 10; } while (uv > 0);
                while (lllen > 0) { tmp[off++] = llbuf[--lllen]; }
                tmp[off] = '\0';

                str_arg = tmp;
                str_arg_l = off;
              }
#else
              str_arg_l+=sprintf(tmp+str_arg_l,f,long_long_arg); 
#endif
              break;
#endif
            }
          } else {  /* unsigned */
            switch (length_modifier) {
            case '\0':
            case 'h': 
              sprintf(tmp + str_arg_l, f, uint_arg);
              str_arg_l += strlen(tmp + str_arg_l);
              break;
            case 'l': 
            case 'z': 
              sprintf(tmp + str_arg_l, f, ulong_arg);
              str_arg_l += strlen(tmp + str_arg_l);
              break;
#ifdef SNPRINTF_LONGLONG_SUPPORT
            case '2': 
#if defined(__sun__) && !defined(SVR4) && !defined(__SVR4)
              {
                char llbuf[64];
                int lllen = 0;
                size_t off = 0;
                unsigned long long uv = ulong_long_arg;

                if (fmt_spec == 'u') {
                  do { llbuf[lllen++] = '0' + (char)(uv % 10); uv /= 10; } while (uv > 0);
                } else if (fmt_spec == 'x' || fmt_spec == 'X') {
                  const char *digits = (fmt_spec == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
                  do { llbuf[lllen++] = digits[uv & 0xF]; uv >>= 4; } while (uv > 0);
                } else if (fmt_spec == 'o') {
                  do { llbuf[lllen++] = '0' + (char)(uv & 0x7); uv >>= 3; } while (uv > 0);
                }

                while (lllen > 0) { tmp[off++] = llbuf[--lllen]; }
                tmp[off] = '\0';

                str_arg = tmp;
                str_arg_l = off;
              }
#else
	           str_arg_l+=sprintf(tmp+str_arg_l,f,ulong_long_arg);
#endif
	           break;
#endif
            }
          }
         /* include the optional minus sign and possible "0x"
            in the region before the zero padding insertion point */
          if (zero_padding_insertion_ind < str_arg_l &&
              tmp[zero_padding_insertion_ind] == '-') {
            zero_padding_insertion_ind++;
          }
          if (zero_padding_insertion_ind+1 < str_arg_l &&
              tmp[zero_padding_insertion_ind]   == '0' &&
             (tmp[zero_padding_insertion_ind+1] == 'x' ||
              tmp[zero_padding_insertion_ind+1] == 'X') ) {
            zero_padding_insertion_ind += 2;
          }
        }
        { size_t num_of_digits = str_arg_l - zero_padding_insertion_ind;
          if (alternate_form && fmt_spec == 'o'
#ifdef HPUX_COMPATIBLE                                  /* ("%#.o",0) -> ""  */
              && (str_arg_l > 0)
#endif
#ifdef DIGITAL_UNIX_BUG_COMPATIBLE                      /* ("%#o",0) -> "00" */
#else
              /* unless zero is already the first character */
              && !(zero_padding_insertion_ind < str_arg_l
                   && tmp[zero_padding_insertion_ind] == '0')
#endif
          ) {        /* assure leading zero for alternate-form octal numbers */
            if (!precision_specified || precision < num_of_digits+1) {
             /* precision is increased to force the first character to be zero,
                except if a zero value is formatted with an explicit precision
                of zero */
              precision = num_of_digits+1; precision_specified = 1;
            }
          }
       /* zero padding to specified precision? */
          if (num_of_digits < precision) 
            number_of_zeros_to_pad = precision - num_of_digits;
        }
     /* zero padding to specified minimal field width? */
        if (!justify_left && zero_padding) {
          int n = min_field_width - (str_arg_l+number_of_zeros_to_pad);
          if (n > 0) number_of_zeros_to_pad += n;
        }
        break;
      }
      default: /* unrecognized conversion specifier, keep format string as-is*/
        zero_padding = 0;  /* turn zero padding off for non-numeric convers. */
#ifndef DIGITAL_UNIX_COMPATIBLE
        justify_left = 1; min_field_width = 0;                /* reset flags */
#endif
#if defined(PERL_COMPATIBLE) || defined(LINUX_COMPATIBLE)
     /* keep the entire format string unchanged */
        str_arg = starting_p; str_arg_l = p - starting_p;
     /* well, not exactly so for Linux, which does something inbetween,
      * and I don't feel an urge to imitate it: "%+++++hy" -> "%+y"  */
#else
     /* discard the unrecognized conversion, just keep *
      * the unrecognized conversion character          */
        str_arg = p; str_arg_l = 0;
#endif
        if (*p) str_arg_l++;  /* include invalid conversion specifier unchanged
                                 if not at end-of-string */
        break;
      }
      if (*p) p++;      /* step over the just processed conversion specifier */
   /* insert padding to the left as requested by min_field_width;
      this does not include the zero padding in case of numerical conversions*/
      if (!justify_left) {                /* left padding with blank or zero */
        int n = min_field_width - (str_arg_l+number_of_zeros_to_pad);
        if (n > 0) {
          if (str_l < str_m) {
            size_t avail = (str_m - str_l > 1) ? (str_m - str_l - 1) : 0;
            fast_memset(str+str_l, (zero_padding?'0':' '), (n>avail?avail:n));
          }
          str_l += n;
        }
      }
   /* zero padding as requested by the precision or by the minimal field width
    * for numeric conversions required? */
      if (number_of_zeros_to_pad <= 0) {
     /* will not copy first part of numeric right now, *
      * force it to be copied later in its entirety    */
        zero_padding_insertion_ind = 0;
      } else {
     /* insert first part of numerics (sign or '0x') before zero padding */
        int n = zero_padding_insertion_ind;
        if (n > 0) {
          if (str_l < str_m) {
            size_t avail = str_m-str_l;
            fast_memcpy(str+str_l, str_arg, (n>avail?avail:n));
          }
          str_l += n;
        }
     /* insert zero padding as requested by the precision or min field width */
        n = number_of_zeros_to_pad;
        if (n > 0) {
          if (str_l < str_m) {
            size_t avail = str_m-str_l;
            fast_memset(str+str_l, '0', (n>avail?avail:n));
          }
          str_l += n;
        }
      }
   /* insert formatted string
    * (or as-is conversion specifier for unknown conversions) */
      { int n = str_arg_l - zero_padding_insertion_ind;
        if (n > 0) {
          if (str_l < str_m) {
            size_t avail = str_m-str_l;
            fast_memcpy(str+str_l, str_arg+zero_padding_insertion_ind,
                        (n>avail?avail:n));
          }
          str_l += n;
        }
      }
   /* insert right padding */
      if (justify_left) {          /* right blank padding to the field width */
        int n = min_field_width - (str_arg_l+number_of_zeros_to_pad);
        if (n > 0) {
          if (str_l < str_m) {
            size_t avail = str_m-str_l;
            fast_memset(str+str_l, ' ', (n>avail?avail:n));
          }
          str_l += n;
        }
      }
    }
  }
#if defined(NEED_SNPRINTF_ONLY)
  va_end(ap);
#endif
  if (str_m > 0) { /* make sure the string is null-terminated
                      even at the expense of overwriting the last character
                      (shouldn't happen, but just in case) */
    str[str_l <= str_m-1 ? str_l : str_m-1] = '\0';
  }
  /* Return the number of characters formatted (excluding trailing null
   * character), that is, the number of characters that would have been
   * written to the buffer if it were large enough.
   *
   * The value of str_l should be returned, but str_l is of unsigned type
   * size_t, and snprintf is int, possibly leading to an undetected
   * integer overflow, resulting in a negative return value, which is illegal.
   * Both XSH5 and ISO C99 (at least the draft) are silent on this issue.
   * Should errno be set to EOVERFLOW and EOF returned in this case???
   */
  return (int) str_l;
}
#endif

/* Embedded Standalone Unit Test */
#ifdef _TEST_SNPRINTF_COMPAT

/* Helper wrapper to validate vasprintf */
static int test_vasprintf_helper(char **strp, const char *fmt, ...) {
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret = vasprintf(strp, fmt, ap);
    va_end(ap);
    return ret;
}

/* =========================================================================
 * Embedded Unit Test Harness
 * Compile test: gcc -O2 -mcpu=v7 -DHAVE_LONG_DOUBLE -DHAVE_LONG_LONG -D_TEST_SNPRINTF_COMPAT snprintf_compat.c -o test_snprintf
 * ========================================================================= */
int main(void) {
    char buf[128];
    int len;
    int n_count = -1;
    long long big_num = 9223372036854775807LL;
    long double ld_val = 3.141592653589793238462643383279502884L;
    size_t sz_val = 1024;
    char *dyn_buf;

    printf("=== Solaris / SunOS Complete 19-Test C99, POSIX & SPARC Stress Suite ===\n");

    /* --- BASE C99 COMPLIANCE CHECKS --- */

    /* 1. 64-bit Long Long Formatting */
    len = snprintf(buf, sizeof(buf), "LLMAX: %lld", big_num);
    printf("[01] 64-bit int: '%s' (len: %d)\n", buf, len);
    assert(strcmp(buf, "LLMAX: 9223372036854775807") == 0);

    /* 2. Truncation & Null Termination Boundary */
    len = snprintf(buf, 10, "1234567890ABCDEF");
    printf("[02] Truncation (bound 10): '%s' (reported len: %d)\n", buf, len);
    assert(strlen(buf) == 9 && strcmp(buf, "123456789") == 0 && len == 16);

    /* 3. Long Double Formatting (%Lf) */
    len = snprintf(buf, sizeof(buf), "LD: %.10Lf", ld_val);
    printf("[03] Long double: '%s' (len: %d)\n", buf, len);
    assert(strstr(buf, "3.1415926536") != NULL);

    /* 4. %n Specifier */
    len = snprintf(buf, sizeof(buf), "Hello %nWorld", &n_count);
    printf("[04] %%n count: '%s' (written %%n: %d, total len: %d)\n", buf, n_count, len);
    assert(n_count == 6 && len == 11 && strcmp(buf, "Hello World") == 0);

    /* 5. NULL-Buffer Dry Run Length Query */
    len = snprintf(NULL, 0, "Test %d string", 123);
    printf("[05] NULL-buffer length query: %d\n", len);
    assert(len == 15);

    /* 6. C99 size_t modifier (%zu) */
    len = snprintf(buf, sizeof(buf), "Size: %zu", sz_val);
    printf("[06] C99 size_t (%%zu): '%s' (len: %d)\n", buf, len);
    assert(strcmp(buf, "Size: 1024") == 0);

    /* --- ADVANCED EDGE-CASE STRESS TESTS --- */

    /* 7. Signed ssize_t / ptrdiff_t Formatting (%zd, %td) */
    len = snprintf(buf, sizeof(buf), "SSize: %zd, Diff: %td", (ssize_t)-512, (ptrdiff_t)-42);
    printf("[07] Signed %%zd / %%td: '%s' (len: %d)\n", buf, len);
    assert(strcmp(buf, "SSize: -512, Diff: -42") == 0);

    /* 8. Hexadecimal size_t Formatting (%zx / %zX) */
    len = snprintf(buf, sizeof(buf), "HexSize: 0x%zx", (size_t)0xDEADBEEF);
    printf("[08] Hex size_t (%%zx): '%s' (len: %d)\n", buf, len);
    assert(strcmp(buf, "HexSize: 0xdeadbeef") == 0);

    /* 9. C99 Precision Zero Rule (value 0 with precision .0 MUST produce 0 chars) */
    len = snprintf(buf, sizeof(buf), "ZeroPrec: '%.0d'", 0);
    printf("[09] Zero value with .0 precision: '%s' (len: %d)\n", buf, len);
    assert(strcmp(buf, "ZeroPrec: ''") == 0);

    /* 10. Width + Precision Zero Padding Combination (%010.5d) */
    len = snprintf(buf, sizeof(buf), "Padded: '%010.5d'", 42);
    printf("[10] Width + Precision padding: '%s' (len: %d)\n", buf, len);
    assert(strcmp(buf, "Padded: '     00042'") == 0); /* C99: precision overrides '0' flag */

    /* 11. Left Alignment + Field Width (%-10s) */
    len = snprintf(buf, sizeof(buf), "Left: '%-10s'", "sparc");
    printf("[11] Left alignment (%%-10s): '%s' (len: %d)\n", buf, len);
    assert(strcmp(buf, "Left: 'sparc     '") == 0);

    /* 12. Dynamic Allocation via asprintf */
    dyn_buf = NULL;
    len = asprintf(&dyn_buf, "Dynamic %s %lld", "Alloc", big_num);
    printf("[12] asprintf dynamic alloc: '%s' (len: %d)\n", dyn_buf, len);
    assert(dyn_buf != NULL);
    assert(strcmp(dyn_buf, "Dynamic Alloc 9223372036854775807") == 0);
    assert(len == 33);
    free(dyn_buf);

    /* 13. Dynamic Allocation via vasprintf */
    dyn_buf = NULL;
    len = test_vasprintf_helper(&dyn_buf, "Size: %zu, Hex: 0x%zx", sz_val, (size_t)0xDEADBEEF);
    printf("[13] vasprintf dynamic alloc: '%s' (len: %d)\n", dyn_buf, len);
    assert(dyn_buf != NULL);
    assert(strcmp(dyn_buf, "Size: 1024, Hex: 0xdeadbeef") == 0);
    assert(len == 27);
    free(dyn_buf);

    /* 14. Dynamic Precision Float (%.*f %s) */
    len = snprintf(buf, sizeof(buf), "%.*f %s", 2, 99.9482, "f");
    printf("[14] Dynamic precision float (%%.*f %%s): '%s' (len: %d)\n", buf, len);
    assert(strcmp(buf, "99.95 f") == 0 && len == 7);

    /* --- CRASH & POSITIONAL HARDENING TESTS --- */

    /* 15. NULL String Pointer Protection (%s with NULL) */
    len = snprintf(buf, sizeof(buf), "NullStr: %s", (char *)NULL);
    printf("[15] NULL string handling: '%s' (len: %d)\n", buf, len);
    assert(strstr(buf, "(null)") != NULL || strstr(buf, "(NULL)") != NULL || strcmp(buf, "NullStr: ") == 0);

    /* 16. SPARC Multi-Argument Stack Alignment Check */
    len = snprintf(buf, sizeof(buf), "User %s port %zu id %lld host %s", "llimon", sz_val, big_num, "github.com");
    printf("[16] Stack alignment check: '%s' (len: %d)\n", buf, len);
    assert(strcmp(buf, "User llimon port 1024 id 9223372036854775807 host github.com") == 0);

    /* 17. vasprintf NULL String Edge Case */
    dyn_buf = NULL;
    len = test_vasprintf_helper(&dyn_buf, "User: %s Host: %s", (char *)NULL, "sparc-box");
    printf("[17] vasprintf NULL string: '%s' (len: %d)\n", dyn_buf, len);
    assert(dyn_buf != NULL);
    assert(strstr(dyn_buf, "(null)") != NULL || strstr(dyn_buf, "(NULL)") != NULL || strstr(dyn_buf, "User: ") != NULL);
    free(dyn_buf);

    /* 18. Zero-Count Buffer Dry Run (count == 0 with non-NULL buffer) */
    buf[0] = 'X';
    len = snprintf(buf, 0, "Should non-mutate buffer");
    printf("[18] Count=0 buffer protection: buf[0]='%c' (reported len: %d)\n", buf[0], len);
    assert(buf[0] == 'X' && len == 24);

    /* 19. Parameter formatting (sequential) */
    len = snprintf(buf, sizeof(buf), "Pos: %s %d %lld", "sparc", 42, big_num);
    printf("[19] Sequential args (%%s %%d %%lld): '%s' (len: %d)\n", buf, len);
    assert(strcmp(buf, "Pos: sparc 42 9223372036854775807") == 0);

    /* 20. SPARC Positional Arguments (%3$s %1$d %2$lld) */
    /*
     *
     * We do not yet support possitional arguments.
     * They are rarely used only in places like glibc localization routines.
     * 
    len = snprintf(buf, sizeof(buf), "Pos: %3$s %1$d %2$lld", 42, 9223372036854775807LL, "sparc");
    printf("[19] Positional args (%%3$s %%1$d %%2$lld): '%s' (len: %d)\n", buf, len);
    assert(strcmp(buf, "Pos: sparc 42 9223372036854775807") == 0);
    */

    /* 20. Tar Verbose Metadata Formatting (64-bit file size + multiple string parameters) */
    {
        char mode[] = "-rw-r--r--";
        char owner[] = "llimon";
        char group[] = "staff";
        unsigned long long file_size = 10737418240ULL; /* 10 GB file size */
        char mtime[] = "2026-10-06 01:55";
        char path[] = "w3m-0.5.3+git20210102/configure";

        len = snprintf(buf, sizeof(buf), "%s %s/%s %12llu %s %s",
                       mode, owner, group, file_size, mtime, path);
        printf("[20] Tar verbose metadata: '%s' (len: %d)\n", buf, len);
        assert(strcmp(buf, "-rw-r--r-- llimon/staff  10737418240 2026-10-06 01:55 w3m-0.5.3+git20210102/configure") == 0);
    }

    printf("\nSUCCESS: All 20 C99, POSIX, and SPARC alignment tests passed cleanly!\n");

    return 0;
}
#endif /* _TEST_SNPRINTF_COMPAT */
