#ifndef __dbg_h__
#define __dbg_h__

#define check(A, M, ...) if(!(A)) { PyErr_Format(_tnetstring_Error,M, ##__VA_ARGS__); goto error; }

#define sentinel(M, ...)  check(0, M, ##__VA_ARGS__)

#define check_mem(A) check((A), "Out of memory.")

#endif
