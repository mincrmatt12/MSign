#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>

int __attribute__((used)) _close(int file) {return -1;}
int __attribute__((used)) _fstat(int file, struct stat *st) {
	st->st_mode = S_IFCHR;
	return 0;
}

int __attribute__((used)) _isatty(int file) {return 1;}

int __attribute__((used)) _lseek(int file, int ptr, int dir) {return 0;}

int __attribute__((used)) _open(const char *name, int flags, int mode) {return -1;}

int __attribute__((used)) _read(int file, char *ptr, int len) {
	// Read len bytes from the debug UART3

	return 0;
}

caddr_t __attribute__((used)) _sbrk(int incr)
{
	extern char end asm("end");
	static char *heap_end;
	char *prev_heap_end,*min_stack_ptr;

	if (heap_end == 0)
		heap_end = &end;

	prev_heap_end = heap_end;

	/* Use the NVIC offset register to locate the main stack pointer. */
	min_stack_ptr = (char*)(*(unsigned int *)*(unsigned int *)0xE000ED08);
	/* Locate the STACK bottom address */
	min_stack_ptr -= 100;

	if (heap_end + incr > min_stack_ptr)
	{
		errno = ENOMEM;
		return (caddr_t) -1;
	}

	heap_end += incr;

	return (caddr_t) prev_heap_end;
}

void __attribute__((used)) _exit(int status) {
	while (1) {;}
}

int __attribute__((used)) _kill(int a, int b) {
	errno = EINVAL;
	return -1;
}

int __attribute__((used)) _getpid() {
	return 1;
}
