#include <cstdio>

#ifndef FORJA_PARITY_ENTRY
 #error "FORJA_PARITY_ENTRY must name the model's parity run function"
#endif

bool FORJA_PARITY_ENTRY();

int main()
{
    return FORJA_PARITY_ENTRY() ? 0 : 1;
}
