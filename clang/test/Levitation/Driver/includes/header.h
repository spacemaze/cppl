#ifndef HEADER
#define HEADER

// See, bug #55
// Only exactly "__const" definition triggers this crash.
//
// Quoted from /usr/include/sys/cdefs.h:116
#define	__const		const		/* define reserved names to standard */

#endif
