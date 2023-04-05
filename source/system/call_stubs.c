#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <stdbool.h>

#include "lpc11xx.h"
#include "bitfields.h"

extern int errno;
extern int __io_getchar(void) __attribute__((weak));

register char* stack_ptr asm("sp");

char* __env[1] = { 0 };
char** environ = __env;

caddr_t _sbrk(int incr)
{
    extern char end asm("end");
    static char* heap_end;
    char* prev_heap_end;

    if (heap_end == 0)
        heap_end = &end;

    prev_heap_end = heap_end;
    if (heap_end + incr > stack_ptr) {
        errno = ENOMEM;
        return (caddr_t)-1;
    }

    heap_end += incr;

    return (caddr_t)prev_heap_end;
}

int __io_putchar(int c)
{
    while (!(LPC_UART->LSR & U0LSR_THRE))
        ;
    LPC_UART->THR = c;
    return 0;
}

void initialise_monitor_handles()
{}

int _getpid(void)
{
    return 1;
}

int _kill(__attribute__((unused)) int pid, __attribute__((unused)) int sig)
{
    errno = EINVAL;
    return -1;
}

void _exit(int status)
{
    _kill(status, -1);
    while (1) {
    } /* Make sure we hang here */
}

__attribute__((weak)) int _read(__attribute__((unused)) int file, char* ptr, int len)
{
    int DataIdx;

    for (DataIdx = 0; DataIdx < len; DataIdx++) {
        *ptr++ = __io_getchar();
    }

    return len;
}

__attribute__((weak)) int _write(__attribute__((unused)) int file, char* ptr, int len)
{
    int DataIdx;

    for (DataIdx = 0; DataIdx < len; DataIdx++) {
        __io_putchar(*ptr++);
    }
    return len;
}

int _close(__attribute__((unused)) int file)
{
    return -1;
}

int _fstat(__attribute__((unused)) int file, struct stat* st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(__attribute__((unused)) int file)
{
    return 1;
}

int _lseek(__attribute__((unused)) int file, __attribute__((unused)) int ptr, __attribute__((unused)) int dir)
{
    return 0;
}

int _open(__attribute__((unused)) char* path, __attribute__((unused)) int flags, ...)
{
    /* Pretend like we always fail */
    return -1;
}

int _wait(__attribute__((unused)) int* status)
{
    errno = ECHILD;
    return -1;
}

int _unlink(__attribute__((unused)) char* name)
{
    errno = ENOENT;
    return -1;
}

int _times(__attribute__((unused)) struct tms* buf)
{
    return -1;
}

int _stat(__attribute__((unused)) char* file, struct stat* st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int _link(__attribute__((unused)) char* old, __attribute__((unused)) char* new)
{
    errno = EMLINK;
    return -1;
}

int _fork(void)
{
    errno = EAGAIN;
    return -1;
}

int _execve(__attribute__((unused)) char* name, __attribute__((unused)) char** argv, __attribute__((unused)) char** env)
{
    errno = ENOMEM;
    return -1;
}

static void __attribute__((constructor)) uart_init(void)
{
    // see sample 13.5.15.1.2 from user manual
    // uart clock = 12 MHz
    // desired baudrate = 115200
    // DIVADDVAL = 5
    // MULVAL = 8
    // DLM = 0
    // DLL = 4
    LPC_UART->LCR |= U0LCR_DLAB;
    LPC_UART->DLM = 0;
    LPC_UART->DLL = 4;
    LPC_UART->LCR &= ~U0LCR_DLAB;
    LPC_UART->FDR = (8 << 4) | (5);

    LPC_UART->LCR = (3 << U0LCR_WLS_SHIFT) & U0LCR_WLS_MASK;
    LPC_UART->FCR = U0FCR_FIFOEN;
    LPC_UART->TER = U0TER_TXEN;
}
