#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifdef WIN32
#include <winsock2.h>
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "auth.h"
#include "configparse.h"
#include "debug.h"
#include "keepalive.h"
#include "libs/md4.h"
#include "libs/md5.h"
#include "protocol.h"
#include "retry_policy.h"
#include "libs/sha1.h"

#define BIND_PORT 61440
#define DEST_PORT 61440

static int login_retry_delay_seconds = DRCOM_DEFAULT_LOGIN_RETRY_DELAY_SECONDS;
static volatile sig_atomic_t stop_requested = 0;

void drcom_request_stop(void) {
    stop_requested = 1;
}

int drcom_stop_requested(void) {
    return stop_requested != 0;
}

static void close_udp_socket(int sockfd) {
#ifdef WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif
}

static void logout_before_stop_exit(int sockfd, struct sockaddr_in dest_addr, unsigned char auth_information[]) {
    unsigned char logout_seed[4];

    printf("[Tips] Stop requested. Logging out before exit.\n");
    if (logging_flag) {
        logging("[Tips] Stop requested. Logging out before exit.", NULL, 0);
    }
    if (dhcp_logout_challenge(sockfd, dest_addr, logout_seed) == 0) {
        dhcp_logout(sockfd, dest_addr, logout_seed, auth_information);
    }
}

static void reset_login_retry_delay(void) {
    login_retry_delay_seconds = DRCOM_DEFAULT_LOGIN_RETRY_DELAY_SECONDS;
}

static int current_login_retry_delay(void) {
    return drcom_normalize_login_retry_delay(login_retry_delay_seconds);
}

static void set_login_retry_delay(int seconds) {
    login_retry_delay_seconds = drcom_normalize_login_retry_delay(seconds);
}

static void sleep_before_retry(int seconds) {
    int retry_delay_seconds = drcom_normalize_login_retry_delay(seconds);
    char retry_msg[64];

    printf("Retrying in %d seconds...\n", retry_delay_seconds);
    if (logging_flag) {
        snprintf(retry_msg, sizeof(retry_msg), "Retrying in %d seconds...", retry_delay_seconds);
        logging(retry_msg, NULL, 0);
    }

    sleep(retry_delay_seconds);
}

static int read_system_uptime_seconds(void) {
#ifdef WIN32
    return 0;
#else
    FILE *uptime_file = fopen("/proc/uptime", "r");
    double uptime_seconds = 0.0;

    if (uptime_file == NULL) {
        return 0;
    }

    if (fscanf(uptime_file, "%lf", &uptime_seconds) != 1) {
        fclose(uptime_file);
        return 0;
    }

    fclose(uptime_file);

    if (uptime_seconds < 0.0) {
        return 0;
    }

    return (int)uptime_seconds;
#endif
}

static void maybe_delay_initial_login(void) {
    int uptime_seconds = read_system_uptime_seconds();
    int wait_seconds;
    char wait_msg[160];
    int startup_delay_seconds = drcom_config.startup_delay_seconds;

    if (startup_delay_seconds <= 0 || uptime_seconds <= 0 || uptime_seconds >= startup_delay_seconds) {
        return;
    }

    wait_seconds = startup_delay_seconds - uptime_seconds;
    snprintf(wait_msg,
             sizeof(wait_msg),
             "[Tips] Initial boot grace period is active. Delaying first login attempt for %d seconds after system startup.",
             wait_seconds);
    printf("%s\n", wait_msg);
    if (logging_flag) {
        logging(wait_msg, NULL, 0);
    }

    sleep(wait_seconds);
}

static int extract_retry_after_seconds(const unsigned char *packet, int packet_length) {
    int value = 0;
    int have_digits = 0;
    int index;

    if (packet == NULL || packet_length <= 20) {
        return 0;
    }

    for (index = 20; index < packet_length && packet[index] != 0x00; index++) {
        unsigned char ch = packet[index];

        if (isdigit((unsigned char)ch)) {
            if (value < 100000) {
                value = value * 10 + (ch - '0');
            }
            have_digits = 1;
            continue;
        }

        if (have_digits) {
            int lookahead = index;
            while (lookahead < packet_length && packet[lookahead] == ' ') {
                lookahead++;
            }
            if (lookahead < packet_length && (packet[lookahead] == 's' || packet[lookahead] == 'S')) {
                if (value > 0 && value <= 1800) {
                    return value;
                }
                return 0;
            }
            value = 0;
            have_digits = 0;
        }
    }

    return 0;
}

int dhcp_challenge(int sockfd, struct sockaddr_in addr, unsigned char seed[]) {
    unsigned char challenge_packet[20], recv_packet[1024];
    memset(challenge_packet, 0, 20);
    challenge_packet[0] = 0x01;
    challenge_packet[1] = 0x02;
    challenge_packet[2] = rand() & 0xff;
    challenge_packet[3] = rand() & 0xff;
    challenge_packet[4] = drcom_config.AUTH_VERSION[0];

#ifdef TEST
    unsigned char test[4] = {0x52, 0x6c, 0xe4, 0x00};
    memcpy(seed, test, 4);
    print_packet("[TEST MODE]<PREP SEED> ", seed, 4);
    return 0;
#endif

    reset_login_retry_delay();

    int recv_length = drcom_udp_send_recv_expected_with_retries(sockfd,
                                                               addr,
                                                               challenge_packet,
                                                               20,
                                                               recv_packet,
                                                               1024,
                                                               "[Challenge sent] ",
                                                               "[Challenge recv] ",
                                                               0x02,
                                                               challenge_packet[1],
                                                               challenge_packet[2],
                                                               challenge_packet[3],
                                                               DRCOM_UDP_EXPECT_ANY,
                                                               8);
    if (recv_length < 0) {
        return 1;
    }

    if (recv_packet[0] != 0x02) {
        printf("Bad challenge response received.\n");
        return 1;
    }

    memcpy(seed, &recv_packet[4], 4);
#ifdef DEBUG
    print_packet("<GET SEED> ", seed, 4);
#endif

    return 0;
}

int dhcp_login(int sockfd, struct sockaddr_in addr, unsigned char seed[], unsigned char auth_information[], int try_JLUversion) {
    unsigned int login_packet_size;
    int JLU_padding = 0;
    size_t password_length = strlen(drcom_config.password);
    size_t username_length = strlen(drcom_config.username);
    int use_ror_version = drcom_config.ror_version || try_JLUversion;

    if (password_length > 8) {
        if (try_JLUversion) {
            printf("Start JLU mode.\n");
            if (logging_flag) {
                logging("Start JLU mode.", NULL, 0);
            }
            if (password_length != 16) {
                JLU_padding = (int)(password_length / 4);
            }
        }
    }
    login_packet_size = drcom_dhcp_login_packet_size(password_length, drcom_config.ror_version, try_JLUversion);
    unsigned char login_packet[login_packet_size], recv_packet[1024], MD5A[16], MACxorMD5A[6], MD5B[16], checksum1[8], checksum2[4];
    memset(login_packet, 0, login_packet_size);
    memset(recv_packet, 0, 100);

    // build login-packet
    login_packet[0] = 0x03;
    login_packet[1] = 0x01;
    login_packet[2] = 0x00;
    login_packet[3] = username_length + 20;
    int MD5A_len = 6 + password_length;
    unsigned char MD5A_str[MD5A_len];
    MD5A_str[0] = 0x03;
    MD5A_str[1] = 0x01;
    memcpy(MD5A_str + 2, seed, 4);
    memcpy(MD5A_str + 6, drcom_config.password, password_length);
    MD5(MD5A_str, MD5A_len, MD5A);
    memcpy(login_packet + 4, MD5A, 16);
    memcpy(login_packet + 20, drcom_config.username, username_length);
    memcpy(login_packet + 56, &drcom_config.CONTROLCHECKSTATUS, 1);
    memcpy(login_packet + 57, &drcom_config.ADAPTERNUM, 1);
    uint64_t sum = 0;
    uint64_t mac = 0;
    // unpack
    for (int i = 0; i < 6; i++) {
        sum = (int)MD5A[i] + sum * 256;
    }
    // unpack
    for (int i = 0; i < 6; i++) {
        mac = (int)drcom_config.mac[i] + mac * 256;
    }
    sum ^= mac;
    // pack
    for (int i = 6; i > 0; i--) {
        MACxorMD5A[i - 1] = (unsigned char)(sum % 256);
        sum /= 256;
    }
    memcpy(login_packet + 58, MACxorMD5A, sizeof(MACxorMD5A));
    int MD5B_len = 9 + password_length;
    unsigned char MD5B_str[MD5B_len];
    memset(MD5B_str, 0, MD5B_len);
    MD5B_str[0] = 0x01;
    memcpy(MD5B_str + 1, drcom_config.password, password_length);
    memcpy(MD5B_str + password_length + 1, seed, 4);
    MD5(MD5B_str, MD5B_len, MD5B);
    memcpy(login_packet + 64, MD5B, 16);
    login_packet[80] = 0x01;
    unsigned char host_ip[4];
    if (drcom_parse_ipv4_address(drcom_config.host_ip, host_ip) != 0) {
        fprintf(stderr, "Invalid host_ip value.\n");
        return 1;
    }
    memcpy(login_packet + 81, host_ip, 4);
    unsigned char checksum1_str[101], checksum1_tmp[4] = {0x14, 0x00, 0x07, 0x0b};
    memcpy(checksum1_str, login_packet, 97);
    memcpy(checksum1_str + 97, checksum1_tmp, 4);
    MD5(checksum1_str, 101, checksum1);
    memcpy(login_packet + 97, checksum1, 8);
    memcpy(login_packet + 105, &drcom_config.IPDOG, 1);
    memcpy(login_packet + 110, &drcom_config.host_name, strlen(drcom_config.host_name));
    unsigned char PRIMARY_DNS[4];
    if (drcom_parse_ipv4_address(drcom_config.PRIMARY_DNS, PRIMARY_DNS) != 0) {
        fprintf(stderr, "Invalid PRIMARY_DNS value.\n");
        return 1;
    }
    memcpy(login_packet + 142, PRIMARY_DNS, 4);
    unsigned char dhcp_server[4];
    if (drcom_parse_ipv4_address(drcom_config.dhcp_server, dhcp_server) != 0) {
        fprintf(stderr, "Invalid dhcp_server value.\n");
        return 1;
    }
    memcpy(login_packet + 146, dhcp_server, 4);
    unsigned char OSVersionInfoSize[4] = {0x94};
    unsigned char OSMajor[4] = {0x05};
    unsigned char OSMinor[4] = {0x01};
    unsigned char OSBuild[4] = {0x28, 0x0a};
    unsigned char PlatformID[4] = {0x02};
    if (try_JLUversion) {
        OSVersionInfoSize[0] = 0x94;
        OSMajor[0] = 0x06;
        OSMinor[0] = 0x02;
        OSBuild[0] = 0xf0;
        OSBuild[1] = 0x23;
        PlatformID[0] = 0x02;
        unsigned char ServicePack[40] = {0x33, 0x64, 0x63, 0x37, 0x39, 0x66, 0x35, 0x32, 0x31, 0x32, 0x65, 0x38, 0x31, 0x37, 0x30, 0x61, 0x63, 0x66, 0x61, 0x39, 0x65, 0x63, 0x39, 0x35, 0x66, 0x31, 0x64, 0x37, 0x34, 0x39, 0x31, 0x36, 0x35, 0x34, 0x32, 0x62, 0x65, 0x37, 0x62, 0x31};
        unsigned char hostname[9] = {0x44, 0x72, 0x43, 0x4f, 0x4d, 0x00, 0xcf, 0x07, 0x68};
        memcpy(login_packet + 182, hostname, 9);
        memcpy(login_packet + 246, ServicePack, 40);
    }
    memcpy(login_packet + 162, OSVersionInfoSize, 4);
    memcpy(login_packet + 166, OSMajor, 4);
    memcpy(login_packet + 170, OSMinor, 4);
    memcpy(login_packet + 174, OSBuild, 4);
    memcpy(login_packet + 178, PlatformID, 4);
    if (!try_JLUversion) {
        memcpy(login_packet + 182, &drcom_config.host_os, strlen(drcom_config.host_os));
    }
    memcpy(login_packet + 310, drcom_config.AUTH_VERSION, 2);
    int counter = 312;
    unsigned int ror_padding = 0;
    if (password_length <= 8) {
        ror_padding = 8 - password_length;
    } else {
        if ((password_length - 8) % 2) {
            ror_padding = 1;
        }
        if (try_JLUversion) {
            ror_padding = JLU_padding;
        }
    }
    if (use_ror_version) {
        MD5(MD5A_str, MD5A_len, MD5A);
        login_packet[counter + 1] = password_length;
        counter += 2;
        for (int i = 0, x = 0; i < (int)password_length; i++) {
            x = (int)MD5A[i] ^ (int)drcom_config.password[i];
            login_packet[counter + i] = (unsigned char)(((x << 3) & 0xff) + (x >> 5));
        }
        counter += password_length;
        // print_packet("TEST ", ror, strlen(drcom_config.password));
    } else {
        ror_padding = 2;
    }
    login_packet[counter] = 0x02;
    login_packet[counter + 1] = 0x0c;
    unsigned char checksum2_str[counter + 18];  // [counter + 14 + 4]
    memset(checksum2_str, 0, counter + 18);
    unsigned char checksum2_tmp[6] = {0x01, 0x26, 0x07, 0x11};
    memcpy(checksum2_str, login_packet, counter + 2);
    memcpy(checksum2_str + counter + 2, checksum2_tmp, 6);
    memcpy(checksum2_str + counter + 8, drcom_config.mac, 6);
    sum = 1234;
    uint64_t ret = 0;
    for (int i = 0; i < counter + 14; i += 4) {
        ret = 0;
        // reverse unsigned char array[4]
        for (int j = 4; j > 0; j--) {
            ret = ret * 256 + (int)checksum2_str[i + j - 1];
        }
        sum ^= ret;
    }
    sum = (1968 * sum) & 0xffffffff;
    for (int j = 0; j < 4; j++) {
        checksum2[j] = (unsigned char)(sum >> (j * 8) & 0xff);
    }
    memcpy(login_packet + counter + 2, checksum2, 4);
    memcpy(login_packet + counter + 8, drcom_config.mac, 6);
    login_packet[counter + ror_padding + 14] = 0xe9;
    login_packet[counter + ror_padding + 15] = 0x13;
    if (try_JLUversion) {
        login_packet[counter + ror_padding + 14] = 0x60;
        login_packet[counter + ror_padding + 15] = 0xa2;
    }

#ifdef TEST
    unsigned char test[16] = {0x44, 0x72, 0x63, 0x6f, 0x77, 0x27, 0x20, 0xca, 0xed, 0x05, 0x6e, 0x35, 0xaa, 0x8b, 0x01, 0xfb};
    memcpy(auth_information, test, 16);
    print_packet("[TEST MODE]<PREP AUTH_INFORMATION> ", auth_information, 16);
    return 0;
#endif

    socklen_t addrlen = sizeof(addr);
    sendto(sockfd, login_packet, sizeof(login_packet), 0, (struct sockaddr *)&addr, sizeof(addr));

    if (verbose_flag) {
        print_packet("[Login sent] ", login_packet, sizeof(login_packet));
    }
    if (logging_flag) {
        logging("[Login sent] ", login_packet, sizeof(login_packet));
    }

    int recv_length = drcom_udp_recv_expected_packet(sockfd,
                                                     addr,
                                                     recv_packet,
                                                     1024,
                                                     "[login recv] ",
                                                     0x04,
                                                     0x05,
                                                     DRCOM_UDP_EXPECT_ANY,
                                                     DRCOM_UDP_EXPECT_ANY,
                                                     DRCOM_UDP_EXPECT_ANY,
                                                     DRCOM_UDP_EXPECT_ANY,
                                                     1);
    if (recv_length < 0) {
#ifdef WIN32
        get_lasterror("Failed to recv data");
#else
        perror("Failed to recv data");
#endif
        return 1;
    }

    if (recv_packet[0] == 0x04 && recv_length < 39) {
        printf("Bad short login response received.\n");
        if (logging_flag) {
            logging("Bad short login response received.", NULL, 0);
        }
        return 1;
    }

    if (recv_packet[0] != 0x04) {
        if (verbose_flag) {
            print_packet("[login recv] ", recv_packet, recv_length);
        }
        printf("<<< Login failed >>>\n");
        if (logging_flag) {
            logging("[login recv] ", recv_packet, recv_length);
            logging("<<< Login failed >>>", NULL, 0);
        }
        char err_msg[256];
        if (recv_packet[0] == 0x05) {
            if (recv_length <= 4) {
                set_login_retry_delay(DRCOM_SHORT_GENERIC_RETRY_DELAY_SECONDS);
                strcpy(err_msg, "[Tips] Server returned a short generic credential rejection. This is usually a temporary template-side rejection rather than a confirmed password error. Local retry is 3 seconds so the client can probe the alternate login template quickly.");
            } else {
                int retry_after_seconds = extract_retry_after_seconds(recv_packet, recv_length);
                set_login_retry_delay(drcom_retry_delay_for_login_reply(recv_packet[4], recv_length, retry_after_seconds));
                switch (recv_packet[4]) {
                    case CHECK_MAC:
                        strcpy(err_msg, "[Tips] Someone is using this account with wired.");
                        break;
                    case SERVER_BUSY:
                        strcpy(err_msg, "[Tips] The server is busy, please log back in again.");
                        break;
                    case WRONG_PASS:
                        if (recv_length <= 5) {
                            strcpy(err_msg, "[Tips] Server returned a short generic credential rejection. This is usually a temporary template-side rejection rather than a confirmed password error. Local retry is 3 seconds so the client can probe the alternate login template quickly.");
                        } else {
                            strcpy(err_msg, "[Tips] Account and password not match.");
                        }
                        break;
                    case NOT_ENOUGH:
                        strcpy(err_msg, "[Tips] The cumulative time or traffic for this account has exceeded the limit.");
                        break;
                    case FREEZE_UP:
                        strcpy(err_msg, "[Tips] This account is suspended.");
                        break;
                    case NOT_ON_THIS_IP:
                        strcpy(err_msg, "[Tips] IP address does not match, this account can only be used in the specified IP address.");
                        break;
                    case NOT_ON_THIS_MAC:
                        strcpy(err_msg, "[Tips] MAC address does not match, this account can only be used in the specified IP and MAC address.");
                        break;
                    case TOO_MUCH_IP:
                        strcpy(err_msg, "[Tips] This account has too many IP addresses.");
                        break;
                    case UPDATE_CLIENT:
                        if (retry_after_seconds > 0) {
                            snprintf(err_msg, sizeof(err_msg), "[Tips] Server forced this account offline and reported a cooldown of %d seconds. Local retry follows the reported cooldown.", retry_after_seconds);
                        } else {
                            strcpy(err_msg, "[Tips] The server rejected the current client signature/version. Local retry remains fixed at 60 seconds.");
                        }
                        break;
                    case NOT_ON_THIS_IP_MAC:
                        strcpy(err_msg, "[Tips] This account can only be used on specified MAC and IP address.");
                        break;
                    case MUST_USE_DHCP:
                        strcpy(err_msg, "[Tips] Your PC set up a static IP, please change to DHCP, and then re-login.");
                        break;
                    default:
                        strcpy(err_msg, "[Tips] Unknown error number.");
                        break;
                }
            }
            printf("%s\n", err_msg);
            if (logging_flag) {
                logging(err_msg, NULL, 0);
            }
        }
        return 1;
    } else {
        if (verbose_flag) {
            print_packet("[login recv] ", recv_packet, recv_length);
        }
        printf("<<< Logged in >>>\n");
        if (logging_flag) {
            logging("[login recv] ", recv_packet, recv_length);
            logging("<<< Logged in >>>", NULL, 0);
        }
    }

    memcpy(auth_information, &recv_packet[23], 16);
#ifdef DEBUG
    print_packet("<GET AUTH_INFORMATION> ", auth_information, 16);
#endif

    if (recvfrom(sockfd, recv_packet, 1024, 0, (struct sockaddr *)&addr, &addrlen) >= 0) {
        DEBUG_PRINT(("Get notice packet."));
    }

    return 0;
}

int dhcp_logout_challenge(int sockfd, struct sockaddr_in addr, unsigned char seed[]) {
    unsigned char challenge_packet[20], recv_packet[1024];
    memset(challenge_packet, 0, sizeof(challenge_packet));
    challenge_packet[0] = 0x01;
    challenge_packet[1] = 0x03;
    challenge_packet[2] = rand() & 0xff;
    challenge_packet[3] = rand() & 0xff;
    challenge_packet[4] = drcom_config.AUTH_VERSION[0];

    int recv_length = drcom_udp_send_recv_expected_with_retries(sockfd,
                                                               addr,
                                                               challenge_packet,
                                                               sizeof(challenge_packet),
                                                               recv_packet,
                                                               sizeof(recv_packet),
                                                               "[Logout challenge sent] ",
                                                               "[Logout challenge recv] ",
                                                               0x02,
                                                               challenge_packet[1],
                                                               challenge_packet[2],
                                                               challenge_packet[3],
                                                               DRCOM_UDP_EXPECT_ANY,
                                                               8);
    if (recv_length < 0) {
        return 1;
    }

    if (recv_packet[0] != 0x02) {
        printf("Bad logout challenge response received.\n");
        return 1;
    }

    memcpy(seed, &recv_packet[4], 4);
    return 0;
}

int dhcp_logout(int sockfd, struct sockaddr_in addr, unsigned char seed[], unsigned char auth_information[]) {
    unsigned char logout_packet[DRCOM_DHCP_LOGOUT_PACKET_SIZE], recv_packet[1024];

    drcom_build_dhcp_logout_packet(&drcom_config, seed, auth_information, logout_packet);
    int recv_length = drcom_udp_send_recv_expected_with_retries(sockfd,
                                                               addr,
                                                               logout_packet,
                                                               sizeof(logout_packet),
                                                               recv_packet,
                                                               sizeof(recv_packet),
                                                               "[Logout sent] ",
                                                               "[Logout recv] ",
                                                               0x04,
                                                               DRCOM_UDP_EXPECT_ANY,
                                                               DRCOM_UDP_EXPECT_ANY,
                                                               DRCOM_UDP_EXPECT_ANY,
                                                               DRCOM_UDP_EXPECT_ANY,
                                                               1);
    if (recv_length < 0) {
        return 1;
    }

    if (recv_packet[0] != 0x04) {
        printf("Bad logout response received.\n");
        return 1;
    }

    printf("<<< Logged out >>>\n");
    if (logging_flag) {
        logging("<<< Logged out >>>", NULL, 0);
    }
    return 0;
}

int pppoe_challenge(int sockfd, struct sockaddr_in addr, int *pppoe_counter, unsigned char seed[], unsigned char sip[], int *encrypt_mode) {
    unsigned char challenge_packet[8], recv_packet[1024];
    memset(challenge_packet, 0, 8);
    unsigned char challenge_tmp[5] = {0x07, 0x00, 0x08, 0x00, 0x01};
    memcpy(challenge_packet, challenge_tmp, 5);
    challenge_packet[1] = *pppoe_counter % 0xFF;
    (*pppoe_counter)++;

    sendto(sockfd, challenge_packet, 8, 0, (struct sockaddr *)&addr, sizeof(addr));

    if (verbose_flag) {
        print_packet("[Challenge sent] ", challenge_packet, 8);
    }
    if (logging_flag) {
        logging("[Challenge sent] ", challenge_packet, 8);
    }
#ifdef TEST
    unsigned char test1[4] = {0x26, 0xe6, 0xe1, 0x02};
    unsigned char test2[4] = {0xc0, 0xa8, 0x01, 0x0b};
    memcpy(seed, test1, 4);
    memcpy(sip, test2, 4);
    *encrypt_mode = 1; /* encrypt_mode test switch [0 or 1] */
    print_packet("[TEST MODE]<PREP SEED> ", seed, 4);
    print_packet("[TEST MODE]<PREP SIP> ", sip, 4);
    printf("[TEST MODE]<PREP ENCRYPT_MODE> %d\n", *encrypt_mode);
    return 0;
#endif

    socklen_t addrlen = sizeof(addr);
    if (recvfrom(sockfd, recv_packet, 1024, 0, (struct sockaddr *)&addr, &addrlen) < 0) {
#ifdef WIN32
        get_lasterror("Failed to recv data");
#else
        perror("Failed to recv data");
#endif
        return 1;
    }

    if (verbose_flag) {
        print_packet("[Challenge recv] ", recv_packet, 32);
    }
    if (logging_flag) {
        logging("[Challenge recv] ", recv_packet, 32);
    }

    if (recv_packet[0] != 0x07) {
        printf("Bad challenge response received.\n");
        return 1;
    }
    if (recv_packet[5] != 0x00) {
        *encrypt_mode = 1;
    } else {
        *encrypt_mode = 0;
    }

#ifdef FORCE_ENCRYPT
    *encrypt_mode = 1;
#endif

    memcpy(seed, &recv_packet[8], 4);
    memcpy(sip, &recv_packet[12], 4);
    memcpy(drcom_config.KEEP_ALIVE_VERSION, &recv_packet[28], 2);
#ifdef DEBUG
    print_packet("<GET SEED> ", seed, 4);
    print_packet("<GET SIP> ", sip, 4);
    printf("<GET ENCRYPT_MODE> %d", *encrypt_mode);
#endif

    return 0;
}

int pppoe_login(int sockfd, struct sockaddr_in addr, int *pppoe_counter, unsigned char seed[], unsigned char sip[], int *login_first, int *encrypt_mode, int *encrypt_type) {
    unsigned char login_packet[96], recv_packet[1024];
    memset(login_packet, 0, 96);
    unsigned char login_tmp[5] = {0x07, 0x00, 0x60, 0x00, 0x03};
    memcpy(login_packet, login_tmp, 5);
    login_packet[1] = *pppoe_counter % 0xFF;
    (*pppoe_counter)++;
    memcpy(login_packet + 12, sip, 4);
    if (*login_first) {
        login_packet[17] = 0x62;
    } else {
        login_packet[17] = 0x63;
    }
    memcpy(login_packet + 19, &drcom_config.pppoe_flag, 1);
    memcpy(login_packet + 20, seed, 4);
    unsigned char crc[8] = {0};
    *encrypt_type = seed[0] & 3;
    if (!*encrypt_mode) {
        *encrypt_type = 0;
    }
    gen_crc(seed, *encrypt_type, crc);
    unsigned char crc_tmp[32] = {0};
    memcpy(crc_tmp, login_packet, 32);
    memcpy(crc_tmp + 24, crc, 8);
    uint64_t ret = 0;
    uint64_t sum = 0;
    unsigned char crc2[4] = {0};
    if (*encrypt_type == 0) {
        for (int i = 0; i < 32; i += 4) {
            ret = 0;
            for (int j = 4; j > 0; j--) {
                ret = ret * 256 + (int)crc_tmp[i + j - 1];
            }
            sum ^= ret;
            sum &= 0xffffffff;
        }
        sum = sum * 19680126 & 0xffffffff;
        for (int i = 0; i < 4; i++) {
            crc2[i] = (unsigned char)(sum % 256);
            sum /= 256;
        }
        memcpy(login_packet + 24, crc2, 4);
    } else {
        memcpy(login_packet + 24, crc, 8);
    }
    // login_packet[39] = 0x8b;
    // memcpy(login_packet + 40, sip, 4);
    // unsigned char smask[4] = {0xff, 0xff, 0xff, 0xff};
    // memcpy(login_packet + 44, smask, 4);
    // login_packet[54] = 0x40;

    sendto(sockfd, login_packet, 96, 0, (struct sockaddr *)&addr, sizeof(addr));
    if (verbose_flag) {
        print_packet("[PPPoE_login sent] ", login_packet, 96);
    }
    if (logging_flag) {
        logging("[PPPoE_login sent] ", login_packet, 96);
    }
#ifdef TEST
    return 0;
#endif

    socklen_t addrlen = sizeof(addr);
    if (recvfrom(sockfd, recv_packet, 1024, 0, (struct sockaddr *)&addr, &addrlen) < 0) {
#ifdef WIN32
        get_lasterror("Failed to recv data");
#else
        perror("Failed to recv data");
#endif
        return 1;
    }

    if (verbose_flag) {
        print_packet("[PPPoE_login recv] ", recv_packet, 48);
    }
    if (logging_flag) {
        logging("[PPPoE_login recv] ", recv_packet, 48);
    }

    if (recv_packet[0] != 0x07) {
        printf("Bad pppoe_login response received.\n");
        return 1;
    }

    if (recvfrom(sockfd, recv_packet, 1024, 0, (struct sockaddr *)&addr, &addrlen) >= 0) {
        DEBUG_PRINT(("Get notice packet."));
    }

    return 0;
}

int dogcom(int try_times) {
#ifdef WIN32
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;
    if (WSAStartup(sockVersion, &wsaData) != 0) {
        return 1;
    }
#endif
    int sockfd;

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    if (verbose_flag) {
        printf("You are binding at %s!\n\n", bind_ip);
    }
#ifdef WIN32
    bind_addr.sin_addr.S_un.S_addr = inet_addr(bind_ip);
#else
    bind_addr.sin_addr.s_addr = inet_addr(bind_ip);
#endif
    bind_addr.sin_port = htons(BIND_PORT);

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
#ifdef WIN32
    dest_addr.sin_addr.S_un.S_addr = inet_addr(drcom_config.server);
#else
    dest_addr.sin_addr.s_addr = inet_addr(drcom_config.server);
#endif
    dest_addr.sin_port = htons(DEST_PORT);

    srand(time(NULL));

    // create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
#ifdef WIN32
        get_lasterror("Failed to create socket");
#else
        perror("Failed to create socket");
#endif
        return 1;
    }
    // bind socket
    if (bind(sockfd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
#ifdef WIN32
        get_lasterror("Failed to bind socket");
#else
        perror("Failed to bind socket");
#endif
        close_udp_socket(sockfd);
        return 1;
    }

    // set timeout
#ifdef WIN32
    int timeout = 3000;
#else
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
#endif
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
#ifdef WIN32
        get_lasterror("Failed to set sock opt");
#else
        perror("Failed to set sock opt");
#endif
        close_udp_socket(sockfd);
        return 1;
    }

    maybe_delay_initial_login();
    if (drcom_stop_requested()) {
        close_udp_socket(sockfd);
        return 0;
    }

    // start dogcoming
    if (strcmp(mode, "dhcp") == 0) {
        int login_failed_attempts = 0;
        int try_JLUversion = 0;
        for (int try_counter = 0; try_counter < try_times; try_counter++) {
            if (drcom_stop_requested()) {
                close_udp_socket(sockfd);
                return 0;
            }
            if (eternal_flag) {
                try_counter = 0;
            }
            unsigned char seed[4];
            unsigned char auth_information[16];
            if (dhcp_challenge(sockfd, dest_addr, seed)) {
                if (drcom_stop_requested()) {
                    close_udp_socket(sockfd);
                    return 0;
                }
                sleep_before_retry(current_login_retry_delay());
            } else {
                int reconnect_after_keepalive_failure = 0;
                usleep(200000);  // 0.2 sec
                try_JLUversion = drcom_login_template_for_attempt(drcom_config.jlu_mode, login_failed_attempts);
                if (login_failed_attempts > 2 && !drcom_config.jlu_mode) {
                    if (try_JLUversion == DRCOM_LOGIN_TEMPLATE_STANDARD) {
                        printf("[Tips] Switching back to the standard Windows login template for this retry.\n");
                        if (logging_flag) {
                            logging("[Tips] Switching back to the standard Windows login template for this retry.", NULL, 0);
                        }
                    }
                }
                if (!dhcp_login(sockfd, dest_addr, seed, auth_information, try_JLUversion)) {
                    int keepalive_counter = 0;
                    int keepalive_try_counter = 0;
                    int keepalive_rounds = 0;
                    login_failed_attempts = 0;
                    reset_login_retry_delay();
                    while (1) {
                        if (drcom_stop_requested()) {
                            logout_before_stop_exit(sockfd, dest_addr, auth_information);
                            close_udp_socket(sockfd);
                            return 0;
                        }
                        if (!keepalive_1(sockfd, dest_addr, seed, auth_information)) {
                            int extra_packet_kind = DRCOM_KEEPALIVE_EXTRA_NONE;
                            usleep(200000);  // 0.2 sec
                            if (drcom_config.jlu_mode) {
                                extra_packet_kind = drcom_keepalive_extra_packet_kind(keepalive_rounds);
                            } else if (keepalive_rounds == 0) {
                                extra_packet_kind = DRCOM_KEEPALIVE_EXTRA_FIRST;
                            }
                            if (keepalive_2(sockfd, dest_addr, &keepalive_counter, extra_packet_kind, NULL)) {
                                keepalive_try_counter = drcom_next_keepalive_failure_count(keepalive_try_counter, 0);
                                if (drcom_should_reconnect_after_keepalive_failure(keepalive_try_counter)) {
                                    set_login_retry_delay(DRCOM_DEFAULT_LOGIN_RETRY_DELAY_SECONDS);
                                    printf("[Tips] Keepalive failed repeatedly. Backing off before re-login.\n");
                                    if (logging_flag) {
                                        logging("[Tips] Keepalive failed repeatedly. Backing off before re-login.", NULL, 0);
                                    }
                                    reconnect_after_keepalive_failure = 1;
                                    break;
                                }
                                continue;
                            }
                            keepalive_rounds++;
                            keepalive_try_counter = drcom_next_keepalive_failure_count(keepalive_try_counter, 1);
                            if (verbose_flag) {
                                printf("Keepalive in loop.\n");
                            }
                            if (logging_flag) {
                                logging("Keepalive in loop.", NULL, 0);
                            }
                            sleep(20);
                        } else {
                            keepalive_try_counter = drcom_next_keepalive_failure_count(keepalive_try_counter, 0);
                            if (drcom_should_reconnect_after_keepalive_failure(keepalive_try_counter)) {
                                set_login_retry_delay(DRCOM_DEFAULT_LOGIN_RETRY_DELAY_SECONDS);
                                printf("[Tips] Keepalive failed repeatedly. Backing off before re-login.\n");
                                if (logging_flag) {
                                    logging("[Tips] Keepalive failed repeatedly. Backing off before re-login.", NULL, 0);
                                }
                                reconnect_after_keepalive_failure = 1;
                                break;
                            }
                            continue;
                        }
                    }
                    if (drcom_stop_requested()) {
                        logout_before_stop_exit(sockfd, dest_addr, auth_information);
                        close_udp_socket(sockfd);
                        return 0;
                    }
                    if (reconnect_after_keepalive_failure) {
                        sleep_before_retry(current_login_retry_delay());
                    }
                } else {
                    login_failed_attempts += 1;
                    if (drcom_stop_requested()) {
                        close_udp_socket(sockfd);
                        return 0;
                    }
                    sleep_before_retry(current_login_retry_delay());
                };
            }
        }
    } else if (strcmp(mode, "pppoe") == 0) {
        int pppoe_counter = 0;
        int keepalive_counter = 0;
        unsigned char seed[4], sip[4]; /* pppoe's seed == dhcp's KEEP_ALIVE_VERSION */
        int login_first = 1;
        int first = 1;
        int encrypt_mode = 0;
        int encrypt_type = 0;
        int try_counter = 0;
        while (1) {
            if (drcom_stop_requested()) {
                close_udp_socket(sockfd);
                return 0;
            }
            if (pppoe_challenge(sockfd, dest_addr, &pppoe_counter, seed, sip, &encrypt_mode)) {
                printf("Retrying...\n");
                if (logging_flag) {
                    logging("Retrying...", NULL, 0);
                }
                login_first = 1;
                try_counter++;
                if (eternal_flag) {
                    try_counter = 0;
                }
                if (try_counter >= try_times) {
                    break;
                }
                sleep(5);
                continue;
            } else {
                usleep(200000);  // 0.2 sec
                if (pppoe_login(sockfd, dest_addr, &pppoe_counter, seed, sip, &login_first, &encrypt_mode, &encrypt_type)) {
                    continue;
                } else {
                    login_first = 0;
                    if (keepalive_2(sockfd, dest_addr, &keepalive_counter, first ? DRCOM_KEEPALIVE_EXTRA_FIRST : DRCOM_KEEPALIVE_EXTRA_NONE, &encrypt_type)) {
                        continue;
                    } else {
                        first = 0;
                        if (verbose_flag) {
                            printf("PPPoE in loop.\n");
                        }
                        if (logging_flag) {
                            logging("PPPoE in loop.", NULL, 0);
                        }
                        sleep(10);
                        continue;
                    }
                }
            }
        }
    }

    printf(">>>>> Failed to keep in touch with server, exiting <<<<<\n\n");
    if (logging_flag) {
        logging(">>>>> Failed to keep in touch with server, exiting <<<<<", NULL, 0);
    }
    close_udp_socket(sockfd);
    return 1;
}

void print_packet(char msg[10], unsigned char *packet, int length) {
    printf("%s", msg);
    for (int i = 0; i < length; i++) {
        printf("%02x", packet[i]);
    }
    printf("\n");
}

void logging(char msg[10], unsigned char *packet, int length) {
    FILE *ptr_file;
    if (log_path == NULL || msg == NULL) {
        return;
    }

    ptr_file = fopen(log_path, "a");
    if (ptr_file == NULL) {
        return;
    }

    char *wday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    time_t timep;
    struct tm *p;
    time(&timep);
    p = localtime(&timep);
    if (p == NULL) {
        fclose(ptr_file);
        return;
    }
    fprintf(ptr_file, "[%04d/%02d/%02d %s %02d:%02d:%02d] ",
            (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday, wday[p->tm_wday], p->tm_hour, p->tm_min, p->tm_sec);

    fprintf(ptr_file, "%s", msg);
    for (int i = 0; i < length; i++) {
        fprintf(ptr_file, "%02x", packet[i]);
    }
    fprintf(ptr_file, "\n");

    fclose(ptr_file);
}

#ifdef WIN32
void get_lasterror(char *msg) {
    char err_msg[256];
    err_msg[0] = '\0';
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  WSAGetLastError(),
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  err_msg,
                  sizeof(err_msg),
                  NULL);
    fprintf(stderr, "%s: %s", msg, err_msg);
}
#endif
