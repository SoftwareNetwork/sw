// This file is in public domain.

// C++ Archive Network (CPPAN)

#ifndef CPPAN_H
#define CPPAN_H

#ifndef CPPAN_BUILD

// many vars available from cmake should be detected here

/*******************************************************************************
*
* general
*
******************************************************************************/

#ifndef CPPAN_CONFIG
#define CPPAN_CONFIG ""
#endif

#ifndef CPPAN_EXPORT
#define CPPAN_EXPORT
#endif

#ifndef CPPAN_PROLOG
#define CPPAN_PROLOG
#endif

#ifndef CPPAN_EPILOG
#define CPPAN_EPILOG
#endif

/*******************************************************************************
 *
 * OS
 *
 ******************************************************************************/

#ifdef WIN32
#define CPPAN_WINDOWS 1
#endif

/*******************************************************************************
 *
 * Compiler
 *
 ******************************************************************************/

/******************************************************************************/

#endif /* CPPAN_BUILD */

#endif /* CPPAN_H */
