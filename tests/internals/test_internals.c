
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

void sprintf_new_suite()
{
    char *result;

    result = sprintf_new("Hello %d %s", 123, "World");
    TEST_ASSERT(strcmp(result, "Hello 123 World") == 0);
    free(result);

    result = sprintf_new("A %s", "loooooooooooooooooooooooooong result");
    TEST_ASSERT(strcmp(result, "A loooooooooooooooooooooooooong result") == 0);
    free(result);
}

int compare_int(const void *a, const void *b)
{
    int x = *(int*)a;
    int y = *(int*)b;
    if (x < y) {
        return -1;
    } else if (x > y) {
        return 1;
    } else {
        return 0;
    }
}

void test_insertion_sort_last(size_t n, int *elements, int *expected)
{
    insertion_sort_last(elements, n, sizeof(int), &compare_int);
    TEST_ASSERT(memcmp(elements, expected, n * sizeof(int)) == 0);
}

void insertion_sort_last_suite()
{
    {
        int elements[6] = { 1, 3, 5, 7, 9, 4 };
        int expected[6] = { 1, 3, 4, 5, 7, 9 };
        test_insertion_sort_last(6, elements, expected);
    }

    {
        int elements[6] = { 1, 3, 5, 7, 9, 0 };
        int expected[6] = { 0, 1, 3, 5, 7, 9 };
        test_insertion_sort_last(6, elements, expected);
    }

    {
        int elements[6] = { 1, 3, 5, 7, 9, 10 };
        int expected[6] = { 1, 3, 5, 7, 9, 10 };
        test_insertion_sort_last(6, elements, expected);
    }

    {
        int elements[6] = { 1, 3, 5, 7, 9, 1 };
        int expected[6] = { 1, 1, 3, 5, 7, 9 };
        test_insertion_sort_last(6, elements, expected);
    }

    {
        int elements[6] = { 1, 3, 5, 7, 9, 9 };
        int expected[6] = { 1, 3, 5, 7, 9, 9 };
        test_insertion_sort_last(6, elements, expected);
    }
}

void test_internal_suite()
{
    my_dirname_suite();
    sprintf_new_suite();
    insertion_sort_last_suite();
}

TEST_MAIN(test_internal_suite);
