#ifndef RETRY_POLICY_H_
#define RETRY_POLICY_H_

#define DRCOM_SHORT_GENERIC_RETRY_DELAY_SECONDS 3
#define DRCOM_DEFAULT_LOGIN_RETRY_DELAY_SECONDS 60
#define DRCOM_KEEPALIVE_FAILURE_RECONNECT_THRESHOLD 6
#define DRCOM_JLU_KEEPALIVE_EXTRA_INTERVAL 10

enum {
    DRCOM_LOGIN_TEMPLATE_STANDARD = 0,
    DRCOM_LOGIN_TEMPLATE_JLU = 1
};

enum {
    DRCOM_KEEPALIVE_EXTRA_NONE = 0,
    DRCOM_KEEPALIVE_EXTRA_FIRST = 1,
    DRCOM_KEEPALIVE_EXTRA_PERIODIC = 2
};

struct drcom_profile_defaults {
    unsigned char AUTH_VERSION[2];
    unsigned char KEEP_ALIVE_VERSION[2];
    int jlu_mode;
    int ror_version;
    int startup_delay_seconds;
};

int drcom_normalize_login_retry_delay(int seconds);
int drcom_retry_delay_for_login_reply(unsigned char reply_code, int recv_length, int retry_after_seconds);
int drcom_next_keepalive_failure_count(int current_failures, int keepalive_exchange_succeeded);
int drcom_should_reconnect_after_keepalive_failure(int consecutive_failures);
int drcom_login_template_for_attempt(int jlu_mode, int login_failed_attempts);
int drcom_keepalive_extra_packet_kind(int completed_keepalive_rounds);
int drcom_profile_defaults_for_name(const char *profile, struct drcom_profile_defaults *defaults);

#endif  // RETRY_POLICY_H_
