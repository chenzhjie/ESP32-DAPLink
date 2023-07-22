#include "dap_utility.h"

#define P2(n) n, n ^ 1, n ^ 1, n
#define P4(n) P2(n), P2(n ^ 1), P2(n ^ 1), P2(n)
#define P6(n) P4(n), P4(n ^ 1), P4(n ^ 1), P4(n)

const uint8_t kParityByteTable[256] = {P6(0), P6(1), P6(1), P6(0)};
