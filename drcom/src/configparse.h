#ifndef CONFIGPARSE_H_
#define CONFIGPARSE_H_

#include <ctype.h>
#include <stdlib.h>

#define DRCOM_DEFAULT_STARTUP_DELAY_SECONDS 60

struct config {
    char profile[20];
    char server[20];
    char username[36];
    char password[20];
    unsigned char CONTROLCHECKSTATUS;
    unsigned char ADAPTERNUM;
    char host_ip[20];
    unsigned char IPDOG;
    char host_name[20];
    char PRIMARY_DNS[20];
    char dhcp_server[20];
    unsigned char AUTH_VERSION[2];
    unsigned char mac[6];
    char host_os[20];
    unsigned char KEEP_ALIVE_VERSION[2];
    int ror_version;
    int keepalive1_mod;
    int jlu_mode;
    int startup_delay_seconds;
    unsigned char pppoe_flag;
    unsigned char keep_alive2_flag; /* abandoned */
};

extern struct config drcom_config;
extern int verbose_flag;
extern int logging_flag;
extern int eapol_flag;
extern int eternal_flag;
extern char *log_path;
extern char mode[10];
extern char bind_ip[20];

int config_parse(char *filepath);
static inline int drcom_parse_ipv4_address(const char *value, unsigned char result[4]) {
    const char *cursor = value;

    if (value == NULL || result == NULL || *value == '\0') {
        return 1;
    }

    for (int index = 0; index < 4; index++) {
        char *end = NULL;
        unsigned long parsed;

        if (!isdigit((unsigned char)*cursor)) {
            return 1;
        }

        parsed = strtoul(cursor, &end, 10);
        if (end == cursor || parsed > 255) {
            return 1;
        }

        result[index] = (unsigned char)parsed;
        if (index < 3) {
            if (*end != '.') {
                return 1;
            }
            cursor = end + 1;
        } else if (*end != '\0') {
            return 1;
        }
    }

    return 0;
}

#endif  // CONFIGPARSE_H_
