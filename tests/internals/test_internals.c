
#include "test_common.h"
#include "misc.h"
#include <string.h>
#include <stdlib.h>

void test_my_dirname(char *arg, const char *expected)
{
    char *orig = strdup(arg);

    const char *ret = my_dirname(arg);
    if (strcmp(ret, expected) != 0) {
        printf("Expected my_dirname(`%s') to return `%s' but got `%s'\n", orig, expected, ret);
        failures++;
    }

    free(orig);
}

void my_dirname_suite()
{
    char buf[256];

    strcpy(buf, "/foo/bar/baz");
    test_my_dirname(buf, "/foo/bar");

    strcpy(buf, "/foo/bar");
    test_my_dirname(buf, "/foo");

    strcpy(buf, "/foo");
    test_my_dirname(buf, "/");

    strcpy(buf, "/foo/");
    test_my_dirname(buf, "/foo");

    strcpy(buf, "/");
    test_my_dirname(buf, "/");

    strcpy(buf, "foo");
    test_my_dirname(buf, ".");

    strcpy(buf, "foo/bar");
    test_my_dirname(buf, "foo");

    strcpy(buf, "./foo/bar");
    test_my_dirname(buf, "./foo");

    strcpy(buf, "./foo");
    test_my_dirname(buf, ".");

    strcpy(buf, ".");
    test_my_dirname(buf, "..");
}

TEST_MAIN(my_dirname_suite);
