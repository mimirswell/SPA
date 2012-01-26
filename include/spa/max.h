#ifndef __MAX_H__
#define __MAX_H__

#ifdef ENABLE_MAX

void __attribute__((noinline)) max_message_handler_entry() {}
void __attribute__((noinline)) max_interesting() {}
#define max_make_symbolic( a, b, c ) (klee_make_symbolic( a, b, c ))

#else

#define max_message_handler_entry()
#define max_interesting()
#define max_make_symbolic( a, b, c )

#endif // #ifdef ENABLE_MAX #else

#endif // #ifndef __MAX_H__
