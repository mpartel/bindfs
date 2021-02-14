
#include "test_common.h"
#include "rate_limiter.h"

static const double epsilon = 0.000000000001;

static volatile double time_now = 123123.0;

double test_clock()
{
    return time_now;
}

void computes_correct_sleep_times()
{
    time_now = 123123.0;
    RateLimiter limiter;
    rate_limiter_init(&limiter, 10, &test_clock);

    double sleep_time = rate_limiter_wait_nosleep(&limiter, 30);
    TEST_ASSERT(NEAR(3.0 + rate_limiter_idle_credit, sleep_time, epsilon));
    sleep_time = rate_limiter_wait_nosleep(&limiter, 20);
    TEST_ASSERT(NEAR(5.0 + rate_limiter_idle_credit, sleep_time, epsilon));

    time_now += 0.5;
    sleep_time = rate_limiter_wait_nosleep(&limiter, 30);
    TEST_ASSERT(NEAR(7.5 + rate_limiter_idle_credit, sleep_time, epsilon));

    rate_limiter_destroy(&limiter);
}

void works_after_being_idle()
{
    time_now = 123123.0;
    RateLimiter limiter;
    rate_limiter_init(&limiter, 10, &test_clock);

    double sleep_time = rate_limiter_wait_nosleep(&limiter, 30);
    TEST_ASSERT(NEAR(3.0 + rate_limiter_idle_credit, sleep_time, epsilon));
    time_now += 100;

    sleep_time = rate_limiter_wait_nosleep(&limiter, 20);
    TEST_ASSERT(NEAR(2 + rate_limiter_idle_credit, sleep_time, epsilon));

    rate_limiter_destroy(&limiter);
}

void sleeps_correct_amount()
{
    RateLimiter limiter;
    rate_limiter_init(&limiter, 10, &gettimeofday_clock);

    double start = gettimeofday_clock();
    rate_limiter_wait(&limiter, 5);
    double elapsed = gettimeofday_clock() - start;
    TEST_ASSERT(NEAR(0.5, elapsed, 0.2));

    rate_limiter_destroy(&limiter);
}

void rate_limiter_suite()
{
    computes_correct_sleep_times();
    works_after_being_idle();
    sleeps_correct_amount();
}

TEST_MAIN(rate_limiter_suite)
