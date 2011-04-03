//
//  dbg.h:  minimal checking and debugging functions
//
//  This is a small compatability shim for the Mongrel2 "dbg.h" interface,
//  to make it easier to port code back and forth between the tnetstring
//  implementation in Mongrel2 and this module.
//

#ifndef __dbg_h__
#define __dbg_h__

#define check(A, M, ...) if(!(A)) { PyErr_Format(PyExc_ValueError, M, ##__VA_ARGS__); goto error; }

#define sentinel(M, ...)  check(0, M, ##__VA_ARGS__)

#define check_mem(A) if(A==NULL) { PyErr_SetString(PyExc_MemoryError, "Out of memory."); goto error; }

#endif
