#include <stdbool.h>
#include <stdlib.h>

#include "board/board.h"
#include "log/log.h"

int main(void)
{
    log_init();
    board_init();

    while (1) {
    }
    return 0;
}