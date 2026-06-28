#include "auth.h"
#include "configparse.h"
#include "retry_policy.h"
#include <string.h>

int drcom_normalize_login_retry_delay(int seconds) {
    return seconds > 0 ? seconds : DRCOM_DEFAULT_LOGIN_RETRY_DELAY_SECONDS;
}

int drcom_retry_delay_for_login_reply(unsigned char reply_code, int recv_length, int retry_after_seconds) {
    if (reply_code == WRONG_PASS && recv_length <= 5) {
        return DRCOM_SHORT_GENERIC_RETRY_DELAY_SECONDS;
    }

    if (reply_code == UPDATE_CLIENT && retry_after_seconds > 0) {
        return retry_after_seconds;
    }

    return DRCOM_DEFAULT_LOGIN_RETRY_DELAY_SECONDS;
}

int drcom_next_keepalive_failure_count(int current_failures, int keepalive_exchange_succeeded) {
    if (keepalive_exchange_succeeded) {
        return 0;
    }

    if (current_failures < 0) {
        current_failures = 0;
    }

    return current_failures + 1;
}

int drcom_should_reconnect_after_keepalive_failure(int consecutive_failures) {
    return consecutive_failures >= DRCOM_KEEPALIVE_FAILURE_RECONNECT_THRESHOLD;
}

int drcom_login_template_for_attempt(int jlu_mode, int login_failed_attempts) {
    if (jlu_mode) {
        return DRCOM_LOGIN_TEMPLATE_JLU;
    }

    if (login_failed_attempts > 2 && login_failed_attempts % 2 != 0) {
        return DRCOM_LOGIN_TEMPLATE_JLU;
    }

    return DRCOM_LOGIN_TEMPLATE_STANDARD;
}

int drcom_keepalive_extra_packet_kind(int completed_keepalive_rounds) {
    if (completed_keepalive_rounds <= 0) {
        return DRCOM_KEEPALIVE_EXTRA_FIRST;
    }

    if (completed_keepalive_rounds % DRCOM_JLU_KEEPALIVE_EXTRA_INTERVAL == 0) {
        return DRCOM_KEEPALIVE_EXTRA_PERIODIC;
    }

    return DRCOM_KEEPALIVE_EXTRA_NONE;
}

int drcom_profile_defaults_for_name(const char *profile, struct drcom_profile_defaults *defaults) {
    if (profile == NULL || defaults == NULL) {
        return 1;
    }

    if (strcmp(profile, "jlu-modern") == 0) {
        defaults->AUTH_VERSION[0] = 0x6a;
        defaults->AUTH_VERSION[1] = 0x00;
        defaults->KEEP_ALIVE_VERSION[0] = 0xdc;
        defaults->KEEP_ALIVE_VERSION[1] = 0x02;
        defaults->jlu_mode = 1;
        defaults->ror_version = 1;
        defaults->startup_delay_seconds = DRCOM_DEFAULT_STARTUP_DELAY_SECONDS;
        return 0;
    }

    if (strcmp(profile, "jlu-legacy") == 0) {
        defaults->AUTH_VERSION[0] = 0x68;
        defaults->AUTH_VERSION[1] = 0x00;
        defaults->KEEP_ALIVE_VERSION[0] = 0xdc;
        defaults->KEEP_ALIVE_VERSION[1] = 0x02;
        defaults->jlu_mode = 1;
        defaults->ror_version = 1;
        defaults->startup_delay_seconds = DRCOM_DEFAULT_STARTUP_DELAY_SECONDS;
        return 0;
    }

    if (strcmp(profile, "generic") == 0) {
        defaults->AUTH_VERSION[0] = 0x2c;
        defaults->AUTH_VERSION[1] = 0x00;
        defaults->KEEP_ALIVE_VERSION[0] = 0xdc;
        defaults->KEEP_ALIVE_VERSION[1] = 0x02;
        defaults->jlu_mode = 0;
        defaults->ror_version = 0;
        defaults->startup_delay_seconds = 0;
        return 0;
    }

    return 1;
}
