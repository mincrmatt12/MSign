#ifndef MSN_RAMFUNC_H
#define MSN_RAMFUNC_H

// Helper definitions for ram located functions
#ifndef __clang__
#define RAMFUNC __attribute__((externally_visible, section(".ram_func")))
#else
#define RAMFUNC __attribute__((section(".ram_func")))
#endif


#endif
