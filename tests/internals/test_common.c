
#include "test_common.h"

int failures = 0;

int run_suite(void (*suite)(void)) {
    suite();
    return (failures > 0) ? 1 : 0;
}
