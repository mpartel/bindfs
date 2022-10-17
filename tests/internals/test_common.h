
#ifndef INC_BINDFS_TEST_COMMON_H
#define INC_BINDFS_TEST_COMMON_H

#include <stdio.h>
#include <math.h>

extern int failures;

#define TEST_ASSERT(expr) do { if (!(expr)) { printf("Assertion failed: %s:%d: `%s'\n", __FILE__, __LINE__, #expr); ++failures; } } while (0);
#define NEAR(a, b, eps) (fabs((a) - (b)) < (eps))

int run_suite(void (*suite)());

#define TEST_MAIN(suite) int main() { return run_suite(suite); }

#endif
