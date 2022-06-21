#include <stdbool.h>
#include <stdlib.h>

#include "board/board.h"
#include "log/log.h"

int main(void)
{
    int* p = NULL;
    p = malloc(sizeof(int));
    if (!p)
        *p = 'A' + 3;
    puts("(TM) funktioniert\r");

    log_init();
    board_init();

    while (1) {
    }
    return 0;
}