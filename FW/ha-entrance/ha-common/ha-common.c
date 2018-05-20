#include <stdint.h>
uint16_t gus_trap_line;

void FATAL_TRAP (uint16_t us_line_num) {
    gus_trap_line = us_line_num;
    while(1);
}
