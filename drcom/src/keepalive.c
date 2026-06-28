#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
typedef int socklen_t;
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "auth.h"
#include "configparse.h"
#include "debug.h"
#include "keepalive.h"
#include "libs/md4.h"
#include "libs/md5.h"
#include "libs/sha1.h"
#include "retry_policy.h"

static const unsigned char DRCOM_KEEPALIVE_FIRST_VERSION[2] = {0x0f, 0x27};

static int drcom_expected_byte_matches(unsigned char value, int expected_byte, int alternate_byte) {
    if (expected_byte == DRCOM_UDP_EXPECT_ANY) {
        return 1;
    }

    return value == (unsigned char)expected_byte ||
           (alternate_byte != DRCOM_UDP_EXPECT_ANY && value == (unsigned char)alternate_byte);
}

static int drcom_packet_matches_expected(const unsigned char *packet,
                                         int packet_length,
                                         int expected_first_byte,
                                         int alternate_first_byte,
                                         int expected_second_byte,
                                         int expected_third_byte,
                                         int expected_fourth_byte,
                                         int expected_sixth_byte,
                                         int minimum_length) {
    if (minimum_length > 0 && packet_length < minimum_length) {
        return 0;
    }

    if (expected_first_byte != DRCOM_UDP_EXPECT_ANY) {
        if (packet_length <= 0 || !drcom_expected_byte_matches(packet[0], expected_first_byte, alternate_first_byte)) {
            return 0;
        }
    }

    if (expected_second_byte != DRCOM_UDP_EXPECT_ANY) {
        if (packet_length <= 1 || packet[1] != (unsigned char)expected_second_byte) {
            return 0;
        }
    }

    if (expected_third_byte != DRCOM_UDP_EXPECT_ANY) {
        if (packet_length <= 2 || packet[2] != (unsigned char)expected_third_byte) {
            return 0;
        }
    }

    if (expected_fourth_byte != DRCOM_UDP_EXPECT_ANY) {
        if (packet_length <= 3 || packet[3] != (unsigned char)expected_fourth_byte) {
            return 0;
        }
    }

    if (expected_sixth_byte != DRCOM_UDP_EXPECT_ANY) {
        if (packet_length <= 5 || packet[5] != (unsigned char)expected_sixth_byte) {
            return 0;
        }
    }

    return 1;
}

static int drcom_packet_source_matches(struct sockaddr_in expected_addr, struct sockaddr_in from_addr) {
    return expected_addr.sin_addr.s_addr == from_addr.sin_addr.s_addr &&
           expected_addr.sin_port == from_addr.sin_port;
}

int drcom_udp_send_recv_with_retries(int sockfd,
                                     struct sockaddr_in addr,
                                     const unsigned char *send_packet,
                                     int send_length,
                                     unsigned char *recv_packet,
                                     int recv_length,
                                     char send_msg[10],
                                     char recv_msg[10]) {
    return drcom_udp_send_recv_expected_with_retries(sockfd,
                                                     addr,
                                                     send_packet,
                                                     send_length,
                                                     recv_packet,
                                                     recv_length,
                                                     send_msg,
                                                     recv_msg,
                                                     DRCOM_UDP_EXPECT_ANY,
                                                     DRCOM_UDP_EXPECT_ANY,
                                                     DRCOM_UDP_EXPECT_ANY,
                                                     DRCOM_UDP_EXPECT_ANY,
                                                     DRCOM_UDP_EXPECT_ANY,
                                                     0);
}

int drcom_udp_recv_expected_packet(int sockfd,
                                   struct sockaddr_in addr,
                                   unsigned char *recv_packet,
                                   int recv_length,
                                   char recv_msg[10],
                                   int expected_first_byte,
                                   int alternate_first_byte,
                                   int expected_second_byte,
                                   int expected_third_byte,
                                   int expected_fourth_byte,
                                   int expected_sixth_byte,
                                   int minimum_length) {
    int recv_result = -1;

    while (1) {
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);

        memset(&from_addr, 0, sizeof(from_addr));
        recv_result = recvfrom(sockfd, recv_packet, recv_length, 0, (struct sockaddr *)&from_addr, &from_len);
        if (recv_result < 0) {
            return -1;
        }

        if (verbose_flag) {
            print_packet(recv_msg, recv_packet, recv_result);
        }
        if (logging_flag) {
            logging(recv_msg, recv_packet, recv_result);
        }

        if (drcom_packet_source_matches(addr, from_addr) &&
            drcom_packet_matches_expected(recv_packet,
                                          recv_result,
                                          expected_first_byte,
                                          alternate_first_byte,
                                          expected_second_byte,
                                          expected_third_byte,
                                          expected_fourth_byte,
                                          expected_sixth_byte,
                                          minimum_length)) {
            return recv_result;
        }

        printf("[Tips] Ignoring unexpected UDP response while waiting for current packet.\n");
        if (logging_flag) {
            logging("[Tips] Ignoring unexpected UDP response while waiting for current packet.", NULL, 0);
        }
    }

    return -1;
}

int drcom_udp_send_recv_expected_with_retries(int sockfd,
                                              struct sockaddr_in addr,
                                              const unsigned char *send_packet,
                                              int send_length,
                                              unsigned char *recv_packet,
                                              int recv_length,
                                              char send_msg[10],
                                              char recv_msg[10],
                                              int expected_first_byte,
                                              int expected_second_byte,
                                              int expected_third_byte,
                                              int expected_fourth_byte,
                                              int expected_sixth_byte,
                                              int minimum_length) {
    int attempt;
    int recv_result = -1;

    for (attempt = 0; attempt <= DRCOM_UDP_PACKET_RETRY_COUNT; attempt++) {
        if (sendto(sockfd, send_packet, send_length, 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
#ifdef WIN32
            get_lasterror("Failed to send data");
#else
            perror("Failed to send data");
#endif
            return -1;
        }

        if (verbose_flag) {
            print_packet(send_msg, (unsigned char *)send_packet, send_length);
        }
        if (logging_flag) {
            logging(send_msg, (unsigned char *)send_packet, send_length);
        }

        recv_result = drcom_udp_recv_expected_packet(sockfd,
                                                     addr,
                                                     recv_packet,
                                                     recv_length,
                                                     recv_msg,
                                                     expected_first_byte,
                                                     DRCOM_UDP_EXPECT_ANY,
                                                     expected_second_byte,
                                                     expected_third_byte,
                                                     expected_fourth_byte,
                                                     expected_sixth_byte,
                                                     minimum_length);
        if (recv_result >= 0) {
            return recv_result;
        }

        if (attempt < DRCOM_UDP_PACKET_RETRY_COUNT) {
            printf("[Tips] UDP response timeout. Resending packet (%d/%d).\n", attempt + 1, DRCOM_UDP_PACKET_RETRY_COUNT);
            if (logging_flag) {
                logging("[Tips] UDP response timeout. Resending packet.", NULL, 0);
            }
        }
    }

#ifdef WIN32
    get_lasterror("Failed to recv data");
#else
    perror("Failed to recv data");
#endif
    return -1;
}

int keepalive_1(int sockfd, struct sockaddr_in addr, unsigned char seed[], unsigned char auth_information[]) {
    if (drcom_config.keepalive1_mod) {
        unsigned char keepalive_1_packet1[8] = {0x07, 0x01, 0x08, 0x00, 0x01, 0x00, 0x00, 0x00};
        unsigned char recv_packet1[1024], keepalive_1_packet2[38], recv_packet2[1024];
        memset(keepalive_1_packet2, 0, 38);
#ifdef TEST
        printf("[TEST MODE]IN TEST MODE, PASS\n");
        return 0;
#endif
        while (1) {
            if (drcom_udp_send_recv_expected_with_retries(sockfd,
                                                         addr,
                                                         keepalive_1_packet1,
                                                         8,
                                                         recv_packet1,
                                                         1024,
                                                         "[Keepalive1_packet1 sent] ",
                                                         "[Keepalive1 challenge_recv] ",
                                                         0x07,
                                                         DRCOM_UDP_EXPECT_ANY,
                                                         DRCOM_UDP_EXPECT_ANY,
                                                         DRCOM_UDP_EXPECT_ANY,
                                                         DRCOM_UDP_EXPECT_ANY,
                                                         12) < 0) {
                return 1;
            } else {
                if (recv_packet1[0] == 0x07) {
                    break;
                } else if (recv_packet1[0] == 0x4d) {
                    DEBUG_PRINT(("Get notice packet.\n"));
                    continue;
                } else {
                    printf("Bad keepalive1 challenge response received.\n");
                    return 1;
                }
            }
        }

        unsigned char keepalive1_seed[4] = {0};
        int encrypt_type;
        unsigned char crc[8] = {0};
        memcpy(keepalive1_seed, &recv_packet1[8], 4);
        encrypt_type = keepalive1_seed[0] & 3;
        gen_crc(keepalive1_seed, encrypt_type, crc);
        keepalive_1_packet2[0] = 0xff;
        memcpy(keepalive_1_packet2 + 8, keepalive1_seed, 4);
        memcpy(keepalive_1_packet2 + 12, crc, 8);
        memcpy(keepalive_1_packet2 + 20, auth_information, 16);
        keepalive_1_packet2[36] = rand() & 0xff;
        keepalive_1_packet2[37] = rand() & 0xff;

        if (drcom_udp_send_recv_expected_with_retries(sockfd,
                                                     addr,
                                                     keepalive_1_packet2,
                                                     38,
                                                     recv_packet2,
                                                     1024,
                                                     "[Keepalive1_packet2 sent] ",
                                                     "[Keepalive1 recv] ",
                                                     0x07,
                                                     DRCOM_UDP_EXPECT_ANY,
                                                     DRCOM_UDP_EXPECT_ANY,
                                                     DRCOM_UDP_EXPECT_ANY,
                                                     DRCOM_UDP_EXPECT_ANY,
                                                     1) < 0) {
            return 1;
        } else {
            if (recv_packet2[0] != 0x07) {
                printf("Bad keepalive1 response received.\n");
                return 1;
            }
        }

    } else {
        unsigned char keepalive_1_packet[42], recv_packet[1024], MD5A[16];
        memset(keepalive_1_packet, 0, 42);
        keepalive_1_packet[0] = 0xff;
        int MD5A_len = 6 + strlen(drcom_config.password);
        unsigned char MD5A_str[MD5A_len];
        MD5A_str[0] = 0x03;
        MD5A_str[1] = 0x01;
        memcpy(MD5A_str + 2, seed, 4);
        memcpy(MD5A_str + 6, drcom_config.password, strlen(drcom_config.password));
        MD5(MD5A_str, MD5A_len, MD5A);
        memcpy(keepalive_1_packet + 1, MD5A, 16);
        memcpy(keepalive_1_packet + 20, auth_information, 16);
        keepalive_1_packet[36] = rand() & 0xff;
        keepalive_1_packet[37] = rand() & 0xff;

#ifdef TEST
        printf("[TEST MODE]IN TEST MODE, PASS\n");
        return 0;
#endif

        while (1) {
            if (drcom_udp_send_recv_expected_with_retries(sockfd,
                                                         addr,
                                                         keepalive_1_packet,
                                                         42,
                                                         recv_packet,
                                                         1024,
                                                         "[Keepalive1 sent] ",
                                                         "[Keepalive1 recv] ",
                                                         0x07,
                                                         DRCOM_UDP_EXPECT_ANY,
                                                         DRCOM_UDP_EXPECT_ANY,
                                                         DRCOM_UDP_EXPECT_ANY,
                                                         DRCOM_UDP_EXPECT_ANY,
                                                         1) < 0) {
                return 1;
            } else {
                if (recv_packet[0] == 0x07) {
                    break;
                } else if (recv_packet[0] == 0x4d) {
                    DEBUG_PRINT(("Get notice packet."));
                    continue;
                } else {
                    printf("Bad keepalive1 response received.\n");
                    return 1;
                }
            }
        }
    }

    return 0;
}

void gen_crc(unsigned char seed[], int encrypt_type, unsigned char crc[]) {
    if (encrypt_type == 0) {
        char DRCOM_DIAL_EXT_PROTO_CRC_INIT[4] = {0xc7, 0x2f, 0x31, 0x01};
        char gencrc_tmp[4] = {0x7e};
        memcpy(crc, DRCOM_DIAL_EXT_PROTO_CRC_INIT, 4);
        memcpy(crc + 4, gencrc_tmp, 4);
    } else if (encrypt_type == 1) {
        unsigned char hash[32] = {0};
        MD5(seed, 4, hash);
        crc[0] = hash[2];
        crc[1] = hash[3];
        crc[2] = hash[8];
        crc[3] = hash[9];
        crc[4] = hash[5];
        crc[5] = hash[6];
        crc[6] = hash[13];
        crc[7] = hash[14];
    } else if (encrypt_type == 2) {
        unsigned char hash[32] = {0};
        MD4(seed, 4, hash);
        crc[0] = hash[1];
        crc[1] = hash[2];
        crc[2] = hash[8];
        crc[3] = hash[9];
        crc[4] = hash[4];
        crc[5] = hash[5];
        crc[6] = hash[11];
        crc[7] = hash[12];
    } else if (encrypt_type == 3) {
        unsigned char hash[32] = {0};
        SHA1(seed, 4, hash);
        crc[0] = hash[2];
        crc[1] = hash[3];
        crc[2] = hash[9];
        crc[3] = hash[10];
        crc[4] = hash[5];
        crc[5] = hash[6];
        crc[6] = hash[15];
        crc[7] = hash[16];
    }
}

void keepalive_2_packetbuilder(unsigned char keepalive_2_packet[], int keepalive_counter, int extra_packet_kind, int type, int encrypt_type) {
    keepalive_2_packet[0] = 0x07;
    keepalive_2_packet[1] = keepalive_counter;
    keepalive_2_packet[2] = 0x28;
    keepalive_2_packet[4] = 0x0b;
    keepalive_2_packet[5] = type;
    if (extra_packet_kind == DRCOM_KEEPALIVE_EXTRA_FIRST || extra_packet_kind == DRCOM_KEEPALIVE_EXTRA_PERIODIC) {
        memcpy(keepalive_2_packet + 6, DRCOM_KEEPALIVE_FIRST_VERSION, 2);
    } else {
        memcpy(keepalive_2_packet + 6, drcom_config.KEEP_ALIVE_VERSION, 2);
    }
    keepalive_2_packet[8] = 0x2f;
    keepalive_2_packet[9] = 0x12;
    if (type == 3) {
        unsigned char host_ip[4] = {0};
        if (strcmp(mode, "dhcp") == 0) {
            if (drcom_parse_ipv4_address(drcom_config.host_ip, host_ip) != 0) {
                return;
            }
            memcpy(keepalive_2_packet + 28, host_ip, 4);
        } else if (strcmp(mode, "pppoe") == 0) {
            unsigned char crc[8] = {0};
            gen_crc(keepalive_2_packet, encrypt_type, crc);
            memcpy(keepalive_2_packet + 32, crc, 8);
        }
    }
}

int keepalive_2(int sockfd, struct sockaddr_in addr, int *keepalive_counter, int extra_packet_kind, int *encrypt_type) {
    unsigned char keepalive_2_packet[40], recv_packet[1024], tail[4];

#ifdef TEST
    printf("[TEST MODE]IN TEST MODE, PASS\n");
#else
    if (extra_packet_kind != DRCOM_KEEPALIVE_EXTRA_NONE) {
        int packet_counter = *keepalive_counter % 0xFF;
        // send the first or periodic extra packet
        memset(keepalive_2_packet, 0, 40);
        if (strcmp(mode, "pppoe") == 0) {
            keepalive_2_packetbuilder(keepalive_2_packet, packet_counter, extra_packet_kind, 1, *encrypt_type);
        } else {
            keepalive_2_packetbuilder(keepalive_2_packet, packet_counter, extra_packet_kind, 1, 0);
        }
        (*keepalive_counter)++;

        if (drcom_udp_send_recv_expected_with_retries(sockfd,
                                                     addr,
                                                     keepalive_2_packet,
                                                     40,
                                                     recv_packet,
                                                     1024,
                                                     "[Keepalive2_extra sent] ",
                                                     "[Keepalive2_extra recv] ",
                                                     0x07,
                                                     packet_counter,
                                                     DRCOM_UDP_EXPECT_ANY,
                                                     DRCOM_UDP_EXPECT_ANY,
                                                     DRCOM_UDP_EXPECT_ANY,
                                                     3) < 0) {
            return 1;
        }

        if (recv_packet[0] == 0x07) {
            if (recv_packet[2] == 0x10) {
                if (verbose_flag) {
                    printf("Filepacket received.\n");
                }
            } else if (recv_packet[2] != 0x28) {
                if (verbose_flag) {
                    printf("Bad keepalive2 response received.\n");
                }
                return 1;
            }
        } else {
            printf("Bad keepalive2 response received.\n");
            return 1;
        }
    }
#endif

    // send the first packet
    int first_packet_counter = *keepalive_counter % 0xFF;
    memset(keepalive_2_packet, 0, 40);
    if (strcmp(mode, "pppoe") == 0) {
        keepalive_2_packetbuilder(keepalive_2_packet, first_packet_counter, DRCOM_KEEPALIVE_EXTRA_NONE, 1, *encrypt_type);
    } else {
        keepalive_2_packetbuilder(keepalive_2_packet, first_packet_counter, DRCOM_KEEPALIVE_EXTRA_NONE, 1, 0);
    }
    (*keepalive_counter)++;

#ifdef TEST
    unsigned char test[4] = {0x13, 0x38, 0xe2, 0x11};
    memcpy(tail, test, 4);
    print_packet("[TEST MODE]<PREP TAIL> ", tail, 4);
#else
    if (drcom_udp_send_recv_expected_with_retries(sockfd,
                                                 addr,
                                                 keepalive_2_packet,
                                                 40,
                                                 recv_packet,
                                                 1024,
                                                 "[Keepalive2_A sent] ",
                                                 "[Keepalive2_B recv] ",
                                                 0x07,
                                                 first_packet_counter,
                                                 0x28,
                                                 DRCOM_UDP_EXPECT_ANY,
                                                 0x02,
                                                 20) < 0) {
        return 1;
    }

    if (recv_packet[0] == 0x07) {
        if (recv_packet[2] != 0x28) {
            printf("Bad keepalive2 response received.\n");
            return 1;
        }
    } else {
        printf("Bad keepalive2 response received.\n");
        return 1;
    }
    memcpy(tail, &recv_packet[16], 4);
#endif

#ifdef DEBUG
    print_packet("<GET TAIL> ", tail, 4);
#endif

    // send the third packet
    int third_packet_counter = *keepalive_counter % 0xFF;
    memset(keepalive_2_packet, 0, 40);
    if (strcmp(mode, "pppoe") == 0) {
        keepalive_2_packetbuilder(keepalive_2_packet, third_packet_counter, DRCOM_KEEPALIVE_EXTRA_NONE, 3, *encrypt_type);
    } else {
        keepalive_2_packetbuilder(keepalive_2_packet, third_packet_counter, DRCOM_KEEPALIVE_EXTRA_NONE, 3, 0);
    }
    memcpy(keepalive_2_packet + 16, tail, 4);
    (*keepalive_counter)++;

#ifdef TEST
    printf("[TEST MODE]IN TEST MODE, PASS\n");
    exit(0);
#endif

    if (drcom_udp_send_recv_expected_with_retries(sockfd,
                                                 addr,
                                                 keepalive_2_packet,
                                                 40,
                                                 recv_packet,
                                                 1024,
                                                 "[Keepalive2_C sent] ",
                                                 "[Keepalive2_D recv] ",
                                                 0x07,
                                                 third_packet_counter,
                                                 0x28,
                                                 DRCOM_UDP_EXPECT_ANY,
                                                 0x04,
                                                 6) < 0) {
        return 1;
    }

    if (recv_packet[0] == 0x07) {
        if (recv_packet[2] != 0x28) {
            printf("Bad keepalive2 response received.\n");
            return 1;
        }
    } else {
        printf("Bad keepalive2 response received.\n");
        return 1;
    }

    return 0;
}
