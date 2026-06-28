#include <stdio.h>
#include <string.h>

#include "../auth.h"
#include "../retry_policy.h"

static int expect_int(const char *name, int actual, int expected) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", name, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_profile(const char *profile, unsigned char expected_auth0, int expected_jlu, int expected_ror) {
    struct drcom_profile_defaults defaults;

    if (drcom_profile_defaults_for_name(profile, &defaults) != 0) {
        fprintf(stderr, "%s: expected supported profile\n", profile);
        return 1;
    }

    if (defaults.AUTH_VERSION[0] != expected_auth0 || defaults.AUTH_VERSION[1] != 0x00) {
        fprintf(stderr, "%s: expected auth version %02x00, got %02x%02x\n",
                profile,
                expected_auth0,
                defaults.AUTH_VERSION[0],
                defaults.AUTH_VERSION[1]);
        return 1;
    }

    if (defaults.jlu_mode != expected_jlu || defaults.ror_version != expected_ror) {
        fprintf(stderr, "%s: expected jlu=%d ror=%d, got jlu=%d ror=%d\n",
                profile,
                expected_jlu,
                expected_ror,
                defaults.jlu_mode,
                defaults.ror_version);
        return 1;
    }

    return 0;
}

int main(void) {
    int failures = 0;

    if (expect_int("short rejection retry", drcom_retry_delay_for_login_reply(WRONG_PASS, 5, 0), DRCOM_SHORT_GENERIC_RETRY_DELAY_SECONDS) != 0) {
        return 1;
    }
    if (expect_int("explicit wrong password retry", drcom_retry_delay_for_login_reply(WRONG_PASS, 6, 0), DRCOM_DEFAULT_LOGIN_RETRY_DELAY_SECONDS) != 0) {
        return 1;
    }
    if (expect_int("server cooldown retry", drcom_retry_delay_for_login_reply(UPDATE_CLIENT, 32, 180), 180) != 0) {
        return 1;
    }
    if (expect_int("update client default retry", drcom_retry_delay_for_login_reply(UPDATE_CLIENT, 32, 0), DRCOM_DEFAULT_LOGIN_RETRY_DELAY_SECONDS) != 0) {
        return 1;
    }
    if (expect_int("normalized retry", drcom_normalize_login_retry_delay(0), DRCOM_DEFAULT_LOGIN_RETRY_DELAY_SECONDS) != 0) {
        return 1;
    }

    failures = drcom_next_keepalive_failure_count(failures, 0);
    failures = drcom_next_keepalive_failure_count(failures, 0);
    if (expect_int("keepalive failures accumulate", failures, 2) != 0) {
        return 1;
    }

    failures = drcom_next_keepalive_failure_count(failures, 1);
    if (expect_int("keepalive success resets failures", failures, 0) != 0) {
        return 1;
    }

    if (expect_int("five failures stay connected", drcom_should_reconnect_after_keepalive_failure(5), 0) != 0) {
        return 1;
    }
    if (expect_int("six failures reconnect", drcom_should_reconnect_after_keepalive_failure(6), 1) != 0) {
        return 1;
    }

    if (expect_int("forced JLU login uses JLU template first", drcom_login_template_for_attempt(1, 0), DRCOM_LOGIN_TEMPLATE_JLU) != 0) {
        return 1;
    }
    if (expect_int("generic login starts with standard template", drcom_login_template_for_attempt(0, 0), DRCOM_LOGIN_TEMPLATE_STANDARD) != 0) {
        return 1;
    }
    if (expect_int("generic login falls back to JLU template", drcom_login_template_for_attempt(0, 3), DRCOM_LOGIN_TEMPLATE_JLU) != 0) {
        return 1;
    }
    if (expect_int("first keepalive sends extra packet", drcom_keepalive_extra_packet_kind(0), DRCOM_KEEPALIVE_EXTRA_FIRST) != 0) {
        return 1;
    }
    if (expect_int("middle keepalive skips extra packet", drcom_keepalive_extra_packet_kind(9), DRCOM_KEEPALIVE_EXTRA_NONE) != 0) {
        return 1;
    }
    if (expect_int("tenth keepalive sends periodic extra packet", drcom_keepalive_extra_packet_kind(10), DRCOM_KEEPALIVE_EXTRA_PERIODIC) != 0) {
        return 1;
    }
    if (expect_profile("jlu-modern", 0x6a, 1, 1) != 0) {
        return 1;
    }
    if (expect_profile("jlu-legacy", 0x68, 1, 1) != 0) {
        return 1;
    }
    if (expect_profile("generic", 0x2c, 0, 0) != 0) {
        return 1;
    }

    printf("retry policy smoke tests passed\n");
    return 0;
}
