
#define _XOPEN_SOURCE 700

#include "test_common.h"
#include "misc.h"
#include <string.h>
#include <stdlib.h>

static void arena_suite()
{
    const int iterations = 1000;
    struct arena arena;
    int** pointers = calloc(iterations, sizeof(int*));

    arena_init(&arena);
    for (int i = 0; i < iterations; ++i) {
        int count = 17 * i;
        int* p = arena_malloc(&arena, count * sizeof(int));
        pointers[i] = p;
        for (int j = 0; j < count; ++j) {
            p[j] = j;
        }
    }

    for (int i = 0; i < iterations; ++i) {
        int count = 17 * i;
        int* p = pointers[i];
        for (int j = 0; j < count; ++j) {
            TEST_ASSERT(p[j] == j);
        }
    }

    arena_free(&arena);
    free(pointers);
}

static void test_my_dirname(char *arg, const char *expected)
{
    char *orig = strdup(arg);

    const char *ret = my_dirname(arg);
    if (strcmp(ret, expected) != 0) {
        printf("Expected my_dirname(`%s') to return `%s' but got `%s'\n", orig, expected, ret);
        failures++;
    }

    free(orig);
}

static void my_dirname_suite()
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

static void sprintf_new_suite() {
    char *result;

    result = sprintf_new("Hello %d %s", 123, "World");
    TEST_ASSERT(strcmp(result, "Hello 123 World") == 0);
    free(result);

    result = sprintf_new("A %s", "loooooooooooooooooooooooooong result");
    TEST_ASSERT(strcmp(result, "A loooooooooooooooooooooooooong result") == 0);
    free(result);
}

static int arg_count(const char **argv)
{
    int i = 0;
    while (argv[i] != NULL) {
        ++i;
    }
    return i;
}

static bool keep_opt(const char *opt) {
    return *opt != '\0' && strncmp(opt, "bad", 3) != 0;
}

static char *join_args(int argc, const char** argv) {
    struct memory_block block = MEMORY_BLOCK_INITIALIZER;
    for (int i = 0; i < argc; ++i) {
        int len = strlen(argv[i]);
        append_to_memory_block(&block, argv[i], len);
        if (i + 1 < argc) {
            append_to_memory_block(&block, " ", 1);
        } else {
            append_to_memory_block(&block, "\0", 1);
        }
    }
    return block.ptr;
}

static void filter_o_opts_test(const char **init, const char **expected) {
    int argc = arg_count(init);
    char *joined_input = join_args(argc, init);

    struct arena arena;
    arena_init(&arena);
    char **argv;
    filter_o_opts(keep_opt, argc, init, &argc, &argv, &arena);

    for (int i = 0; i < argc; ++i) {
        if (argv[i] == NULL) {
            printf("Expected %s but got end of argv at index %d with input %s\n", expected[i], i, joined_input);
            failures++;
            break;
        }
        if (expected[i] == NULL) {
            printf("Expected end of args but got %s at index %d with input %s\n", argv[i], i, joined_input);
            failures++;
            break;
        }
        if (strcmp(argv[i], expected[i]) != 0) {
            printf("Expected %s but got %s at index %d with input %s\n", expected[i], argv[i], i, joined_input);
            failures++;
            break;
        }
    }

    arena_free(&arena);
    free(joined_input);
}

static void filter_o_opts_suite() {
    {
        const char *in[] = {"-obad1", NULL};
        const char *exp[] = {NULL};
        filter_o_opts_test(in, exp);
    }

    {
        const char *in[] = {"-ogood1", NULL};
        const char *exp[] = {"-ogood1", NULL};
        filter_o_opts_test(in, exp);
    }
    {
        const char *in[] = {"-obad1,good1", NULL};
        const char *exp[] = {"-ogood1", NULL};
        filter_o_opts_test(in, exp);
    }

    {
        const char *in[] = {"-ogood1,bad", NULL};
        const char *exp[] = {"-ogood1", NULL};
        filter_o_opts_test(in, exp);
    }

    {
        const char *in[] = {"-obad1,good1,bad2", NULL};
        const char *exp[] = {"-ogood1", NULL};
        filter_o_opts_test(in, exp);
    }

    {
        const char *in[] = {"-obad1,good1,bad2,good2", NULL};
        const char *exp[] = {"-ogood1,good2", NULL};
        filter_o_opts_test(in, exp);
    }

    {
        const char *in[] = {"-ogood1,bad1,good2", NULL};
        const char *exp[] = {"-ogood1,good2", NULL};
        filter_o_opts_test(in, exp);
    }

    {
        const char *in[] = {"-o", "bad1", NULL};
        const char *exp[] = {NULL};
        filter_o_opts_test(in, exp);
    }

    {
        const char *in[] = {"-o", "good1", NULL};
        const char *exp[] = {"-ogood1", NULL};
        filter_o_opts_test(in, exp);
    }

    {
        const char *in[] = {"-o", "good1,bad1,good2", NULL};
        const char *exp[] = {"-ogood1,good2", NULL};
        filter_o_opts_test(in, exp);
    }

    {
        const char *in[] = {"-o", "bad1,good1,bad2", NULL};
        const char *exp[] = {"-ogood1", NULL};
        filter_o_opts_test(in, exp);
    }

    {
        const char *in[] = {"unrelated1", "-o", "bad1,good1,bad2", "unrelated2", NULL};
        const char *exp[] = {"unrelated1", "-ogood1", "unrelated2", NULL};
        filter_o_opts_test(in, exp);
    }

    {
        const char *in[] = {"unrelated1", "-o", ",,,bad1,,good1,,bad2,,,", "unrelated2", NULL};
        const char *exp[] = {"unrelated1", "-ogood1", "unrelated2", NULL};
        filter_o_opts_test(in, exp);
    }

    {
        const char *in[] = {"-o", NULL};
        const char *exp[] = {NULL};
        filter_o_opts_test(in, exp);
    }
}

static void test_internal_suite() {
    arena_suite();
    my_dirname_suite();
    sprintf_new_suite();
    filter_o_opts_suite();
}

TEST_MAIN(test_internal_suite);
