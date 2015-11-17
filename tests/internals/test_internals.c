
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

void sprintf_new_suite() {
    char *result;

    result = sprintf_new("Hello %d %s", 123, "World");
    TEST_ASSERT(strcmp(result, "Hello 123 World") == 0);
    free(result);

    result = sprintf_new("A %s", "loooooooooooooooooooooooooong result");
    TEST_ASSERT(strcmp(result, "A loooooooooooooooooooooooooong result") == 0);
    free(result);
}

void test_internal_suite() {
    my_dirname_suite();
    sprintf_new_suite();
}

TEST_MAIN(test_internal_suite);
