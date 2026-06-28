#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../configparse.h"
#include "../keepalive.h"
#include "../retry_policy.h"

struct config drcom_config;
int verbose_flag = 0;
int logging_flag = 0;
char mode[10];
char *log_path = NULL;

void print_packet(char msg[10], unsigned char *packet, int length) {
    (void)msg;
    (void)packet;
    (void)length;
}

void logging(char msg[10], unsigned char *packet, int length) {
    (void)msg;
    (void)packet;
    (void)length;
}

static int expect_true(const char *name, int condition) {
    if (!condition) {
        fprintf(stderr, "assertion failed: %s\n", name);
        return 1;
    }
    return 0;
}

static int expect_keepalive_version(int extra_packet_kind, unsigned char expected0, unsigned char expected1) {
    unsigned char packet[40];

    memset(packet, 0, sizeof(packet));
    keepalive_2_packetbuilder(packet, 7, extra_packet_kind, 1, 0);

    if (expect_true("packet type", packet[5] == 0x01) != 0 ||
        expect_true("keepalive version byte 0", packet[6] == expected0) != 0 ||
        expect_true("keepalive version byte 1", packet[7] == expected1) != 0) {
        return 1;
    }

    return 0;
}

static int test_udp_exchange_retries_after_dropped_reply(void) {
    int server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    struct timeval timeout;
    pid_t child;
    int status = 0;
    unsigned char request[4] = {0xde, 0xad, 0xbe, 0xef};
    unsigned char response[4] = {0};
    int result;

    if (server_fd < 0 || client_fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_port = 0;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    if (getsockname(server_fd, (struct sockaddr *)&server_addr, &server_addr_len) < 0) {
        perror("getsockname");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    child = fork();
    if (child < 0) {
        perror("fork");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    if (child == 0) {
        unsigned char first[4] = {0};
        unsigned char second[4] = {0};
        unsigned char reply[4] = {0x07, 0x00, 0x28, 0x00};
        struct sockaddr_in peer_addr;
        socklen_t peer_len = sizeof(peer_addr);

        close(client_fd);
        if (recvfrom(server_fd, first, sizeof(first), 0, (struct sockaddr *)&peer_addr, &peer_len) != (ssize_t)sizeof(first)) {
            _exit(2);
        }
        if (memcmp(first, request, sizeof(request)) != 0) {
            _exit(3);
        }
        if (recvfrom(server_fd, second, sizeof(second), 0, (struct sockaddr *)&peer_addr, &peer_len) != (ssize_t)sizeof(second)) {
            _exit(4);
        }
        if (memcmp(second, request, sizeof(request)) != 0) {
            _exit(5);
        }
        if (sendto(server_fd, reply, sizeof(reply), 0, (struct sockaddr *)&peer_addr, peer_len) != (ssize_t)sizeof(reply)) {
            _exit(6);
        }
        close(server_fd);
        _exit(0);
    }

    close(server_fd);
    result = drcom_udp_send_recv_with_retries(client_fd, server_addr, request, sizeof(request), response, sizeof(response), "[test sent] ", "[test recv] ");
    close(client_fd);

    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (expect_true("udp helper receives response after retry", result == (int)sizeof(response)) != 0 ||
        expect_true("udp helper response payload", response[0] == 0x07 && response[2] == 0x28) != 0 ||
        expect_true("udp helper child succeeded", WIFEXITED(status) && WEXITSTATUS(status) == 0) != 0) {
        return 1;
    }

    return 0;
}

static int test_udp_exchange_skips_unexpected_packet(void) {
    int server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    struct timeval timeout;
    pid_t child;
    int status = 0;
    unsigned char request[4] = {0xca, 0xfe, 0xba, 0xbe};
    unsigned char response[4] = {0};
    int result;

    if (server_fd < 0 || client_fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_port = 0;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 ||
        getsockname(server_fd, (struct sockaddr *)&server_addr, &server_addr_len) < 0) {
        perror("bind/getsockname");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    child = fork();
    if (child < 0) {
        perror("fork");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    if (child == 0) {
        unsigned char received[4] = {0};
        unsigned char notice[4] = {0x4d, 0x00, 0x00, 0x00};
        unsigned char reply[4] = {0x07, 0x00, 0x28, 0x00};
        struct sockaddr_in peer_addr;
        socklen_t peer_len = sizeof(peer_addr);

        close(client_fd);
        if (recvfrom(server_fd, received, sizeof(received), 0, (struct sockaddr *)&peer_addr, &peer_len) != (ssize_t)sizeof(received)) {
            _exit(2);
        }
        if (memcmp(received, request, sizeof(request)) != 0) {
            _exit(3);
        }
        if (sendto(server_fd, notice, sizeof(notice), 0, (struct sockaddr *)&peer_addr, peer_len) != (ssize_t)sizeof(notice)) {
            _exit(4);
        }
        if (sendto(server_fd, reply, sizeof(reply), 0, (struct sockaddr *)&peer_addr, peer_len) != (ssize_t)sizeof(reply)) {
            _exit(5);
        }
        close(server_fd);
        _exit(0);
    }

    close(server_fd);
    result = drcom_udp_send_recv_expected_with_retries(client_fd, server_addr, request, sizeof(request), response, sizeof(response), "[test sent] ", "[test recv] ", 0x07, DRCOM_UDP_EXPECT_ANY, 0x28, DRCOM_UDP_EXPECT_ANY, DRCOM_UDP_EXPECT_ANY, sizeof(response));
    close(client_fd);

    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (expect_true("udp helper skips unexpected packet", result == (int)sizeof(response)) != 0 ||
        expect_true("udp helper returned expected packet", response[0] == 0x07 && response[2] == 0x28) != 0 ||
        expect_true("udp helper child succeeded after unexpected packet", WIFEXITED(status) && WEXITSTATUS(status) == 0) != 0) {
        return 1;
    }

    return 0;
}

static int test_udp_exchange_keeps_destination_after_unexpected_source(void) {
    int server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int stray_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    struct timeval client_timeout;
    struct timeval server_timeout;
    pid_t child;
    int status = 0;
    unsigned char request[4] = {0x07, 0x09, 0x28, 0x00};
    unsigned char response[4] = {0};
    int result;

    if (server_fd < 0 || client_fd < 0 || stray_fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_port = 0;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 ||
        getsockname(server_fd, (struct sockaddr *)&server_addr, &server_addr_len) < 0) {
        perror("bind/getsockname");
        close(server_fd);
        close(client_fd);
        close(stray_fd);
        return 1;
    }

    client_timeout.tv_sec = 0;
    client_timeout.tv_usec = 100000;
    server_timeout.tv_sec = 1;
    server_timeout.tv_usec = 0;
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &client_timeout, sizeof(client_timeout)) < 0 ||
        setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &server_timeout, sizeof(server_timeout)) < 0) {
        perror("setsockopt");
        close(server_fd);
        close(client_fd);
        close(stray_fd);
        return 1;
    }

    child = fork();
    if (child < 0) {
        perror("fork");
        close(server_fd);
        close(client_fd);
        close(stray_fd);
        return 1;
    }

    if (child == 0) {
        unsigned char first[4] = {0};
        unsigned char second[4] = {0};
        unsigned char stray[4] = {0x4d, 0x00, 0x00, 0x00};
        unsigned char reply[4] = {0x07, 0x09, 0x28, 0x00};
        struct sockaddr_in peer_addr;
        socklen_t peer_len = sizeof(peer_addr);

        close(client_fd);
        if (recvfrom(server_fd, first, sizeof(first), 0, (struct sockaddr *)&peer_addr, &peer_len) != (ssize_t)sizeof(first)) {
            _exit(2);
        }
        if (sendto(stray_fd, stray, sizeof(stray), 0, (struct sockaddr *)&peer_addr, peer_len) != (ssize_t)sizeof(stray)) {
            _exit(3);
        }
        if (recvfrom(server_fd, second, sizeof(second), 0, (struct sockaddr *)&peer_addr, &peer_len) != (ssize_t)sizeof(second)) {
            _exit(4);
        }
        if (memcmp(first, request, sizeof(request)) != 0 || memcmp(second, request, sizeof(request)) != 0) {
            _exit(5);
        }
        if (sendto(server_fd, reply, sizeof(reply), 0, (struct sockaddr *)&peer_addr, peer_len) != (ssize_t)sizeof(reply)) {
            _exit(6);
        }
        close(server_fd);
        close(stray_fd);
        _exit(0);
    }

    close(server_fd);
    close(stray_fd);
    result = drcom_udp_send_recv_expected_with_retries(client_fd, server_addr, request, sizeof(request), response, sizeof(response), "[test sent] ", "[test recv] ", 0x07, 0x09, 0x28, DRCOM_UDP_EXPECT_ANY, DRCOM_UDP_EXPECT_ANY, sizeof(response));
    close(client_fd);

    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (expect_true("udp helper keeps original destination", result == (int)sizeof(response)) != 0 ||
        expect_true("udp helper response after stray source", response[0] == 0x07 && response[1] == 0x09 && response[2] == 0x28) != 0 ||
        expect_true("udp helper child saw retry at server", WIFEXITED(status) && WEXITSTATUS(status) == 0) != 0) {
        return 1;
    }

    return 0;
}

static int test_udp_exchange_skips_stale_same_type_packet(void) {
    int server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    struct timeval timeout;
    pid_t child;
    int status = 0;
    unsigned char request[4] = {0x07, 0x05, 0x28, 0x00};
    unsigned char response[4] = {0};
    int result;

    if (server_fd < 0 || client_fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_port = 0;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 ||
        getsockname(server_fd, (struct sockaddr *)&server_addr, &server_addr_len) < 0) {
        perror("bind/getsockname");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    child = fork();
    if (child < 0) {
        perror("fork");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    if (child == 0) {
        unsigned char received[4] = {0};
        unsigned char stale[4] = {0x07, 0x04, 0x28, 0x00};
        unsigned char reply[4] = {0x07, 0x05, 0x28, 0x00};
        struct sockaddr_in peer_addr;
        socklen_t peer_len = sizeof(peer_addr);

        close(client_fd);
        if (recvfrom(server_fd, received, sizeof(received), 0, (struct sockaddr *)&peer_addr, &peer_len) != (ssize_t)sizeof(received)) {
            _exit(2);
        }
        if (sendto(server_fd, stale, sizeof(stale), 0, (struct sockaddr *)&peer_addr, peer_len) != (ssize_t)sizeof(stale)) {
            _exit(3);
        }
        if (sendto(server_fd, reply, sizeof(reply), 0, (struct sockaddr *)&peer_addr, peer_len) != (ssize_t)sizeof(reply)) {
            _exit(4);
        }
        close(server_fd);
        _exit(0);
    }

    close(server_fd);
    result = drcom_udp_send_recv_expected_with_retries(client_fd, server_addr, request, sizeof(request), response, sizeof(response), "[test sent] ", "[test recv] ", 0x07, 0x05, 0x28, DRCOM_UDP_EXPECT_ANY, DRCOM_UDP_EXPECT_ANY, sizeof(response));
    close(client_fd);

    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (expect_true("udp helper skips stale same-type packet", result == (int)sizeof(response)) != 0 ||
        expect_true("udp helper returned matching counter", response[0] == 0x07 && response[1] == 0x05 && response[2] == 0x28) != 0 ||
        expect_true("udp helper child succeeded after stale same-type packet", WIFEXITED(status) && WEXITSTATUS(status) == 0) != 0) {
        return 1;
    }

    return 0;
}

static int test_udp_recv_skips_unexpected_without_resend(void) {
    int server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    socklen_t client_addr_len = sizeof(client_addr);
    struct timeval timeout;
    pid_t child;
    int status = 0;
    unsigned char response[8] = {0};
    int result;

    if (server_fd < 0 || client_fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_port = 0;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    client_addr.sin_port = 0;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 ||
        bind(client_fd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0 ||
        getsockname(server_fd, (struct sockaddr *)&server_addr, &server_addr_len) < 0 ||
        getsockname(client_fd, (struct sockaddr *)&client_addr, &client_addr_len) < 0) {
        perror("bind/getsockname");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    child = fork();
    if (child < 0) {
        perror("fork");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    if (child == 0) {
        unsigned char notice[4] = {0x4d, 0x00, 0x00, 0x00};
        unsigned char login_reply[8] = {0x04, 0x00, 0x00, 0x00, 0xaa, 0xbb, 0xcc, 0xdd};

        close(client_fd);
        if (sendto(server_fd, notice, sizeof(notice), 0, (struct sockaddr *)&client_addr, client_addr_len) != (ssize_t)sizeof(notice)) {
            _exit(2);
        }
        if (sendto(server_fd, login_reply, sizeof(login_reply), 0, (struct sockaddr *)&client_addr, client_addr_len) != (ssize_t)sizeof(login_reply)) {
            _exit(3);
        }
        close(server_fd);
        _exit(0);
    }

    close(server_fd);
    result = drcom_udp_recv_expected_packet(client_fd, server_addr, response, sizeof(response), "[test recv] ", 0x04, 0x05, DRCOM_UDP_EXPECT_ANY, DRCOM_UDP_EXPECT_ANY, DRCOM_UDP_EXPECT_ANY, DRCOM_UDP_EXPECT_ANY, 1);
    close(client_fd);

    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (expect_true("udp recv skips unexpected packet without resend", result == (int)sizeof(response)) != 0 ||
        expect_true("udp recv returned login reply", response[0] == 0x04 && response[4] == 0xaa) != 0 ||
        expect_true("udp recv child succeeded", WIFEXITED(status) && WEXITSTATUS(status) == 0) != 0) {
        return 1;
    }

    return 0;
}

static int test_udp_recv_drains_many_unexpected_packets_until_expected_reply(void) {
    int server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    socklen_t client_addr_len = sizeof(client_addr);
    struct timeval timeout;
    pid_t child;
    int status = 0;
    unsigned char response[8] = {0};
    int result;

    if (server_fd < 0 || client_fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_port = 0;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    client_addr.sin_port = 0;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 ||
        bind(client_fd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0 ||
        getsockname(server_fd, (struct sockaddr *)&server_addr, &server_addr_len) < 0 ||
        getsockname(client_fd, (struct sockaddr *)&client_addr, &client_addr_len) < 0) {
        perror("bind/getsockname");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    child = fork();
    if (child < 0) {
        perror("fork");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    if (child == 0) {
        unsigned char notice[4] = {0x4d, 0x00, 0x00, 0x00};
        unsigned char reply[8] = {0x04, 0x00, 0x00, 0x00, 0xaa, 0xbb, 0xcc, 0xdd};
        int index;

        close(client_fd);
        for (index = 0; index < 8; index++) {
            if (sendto(server_fd, notice, sizeof(notice), 0, (struct sockaddr *)&client_addr, client_addr_len) != (ssize_t)sizeof(notice)) {
                _exit(2);
            }
        }
        if (sendto(server_fd, reply, sizeof(reply), 0, (struct sockaddr *)&client_addr, client_addr_len) != (ssize_t)sizeof(reply)) {
            _exit(3);
        }
        close(server_fd);
        _exit(0);
    }

    close(server_fd);
    result = drcom_udp_recv_expected_packet(client_fd, server_addr, response, sizeof(response), "[test recv] ", 0x04, DRCOM_UDP_EXPECT_ANY, DRCOM_UDP_EXPECT_ANY, DRCOM_UDP_EXPECT_ANY, DRCOM_UDP_EXPECT_ANY, DRCOM_UDP_EXPECT_ANY, 1);
    close(client_fd);

    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (expect_true("udp recv drains many unexpected packets", result == (int)sizeof(response)) != 0 ||
        expect_true("udp recv returned expected after drain", response[0] == 0x04 && response[4] == 0xaa) != 0 ||
        expect_true("udp recv drain child succeeded", WIFEXITED(status) && WEXITSTATUS(status) == 0) != 0) {
        return 1;
    }

    return 0;
}

static int test_udp_exchange_matches_challenge_random_echo(void) {
    int server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    struct timeval timeout;
    pid_t child;
    int status = 0;
    unsigned char request[4] = {0x01, 0x02, 0xaa, 0xbb};
    unsigned char response[8] = {0};
    int result;

    if (server_fd < 0 || client_fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_port = 0;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 ||
        getsockname(server_fd, (struct sockaddr *)&server_addr, &server_addr_len) < 0) {
        perror("bind/getsockname");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    child = fork();
    if (child < 0) {
        perror("fork");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    if (child == 0) {
        unsigned char received[4] = {0};
        unsigned char stale[8] = {0x02, 0x02, 0x00, 0x00, 0x11, 0x22, 0x33, 0x44};
        unsigned char reply[8] = {0x02, 0x02, 0xaa, 0xbb, 0x55, 0x66, 0x77, 0x88};
        struct sockaddr_in peer_addr;
        socklen_t peer_len = sizeof(peer_addr);

        close(client_fd);
        if (recvfrom(server_fd, received, sizeof(received), 0, (struct sockaddr *)&peer_addr, &peer_len) != (ssize_t)sizeof(received)) {
            _exit(2);
        }
        if (sendto(server_fd, stale, sizeof(stale), 0, (struct sockaddr *)&peer_addr, peer_len) != (ssize_t)sizeof(stale)) {
            _exit(3);
        }
        if (sendto(server_fd, reply, sizeof(reply), 0, (struct sockaddr *)&peer_addr, peer_len) != (ssize_t)sizeof(reply)) {
            _exit(4);
        }
        close(server_fd);
        _exit(0);
    }

    close(server_fd);
    result = drcom_udp_send_recv_expected_with_retries(client_fd, server_addr, request, sizeof(request), response, sizeof(response), "[test sent] ", "[test recv] ", 0x02, 0x02, 0xaa, 0xbb, DRCOM_UDP_EXPECT_ANY, sizeof(response));
    close(client_fd);

    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (expect_true("udp helper skips stale challenge echo", result == (int)sizeof(response)) != 0 ||
        expect_true("udp helper returned matching challenge echo", response[0] == 0x02 && response[1] == 0x02 && response[2] == 0xaa && response[3] == 0xbb) != 0 ||
        expect_true("udp helper challenge child succeeded", WIFEXITED(status) && WEXITSTATUS(status) == 0) != 0) {
        return 1;
    }

    return 0;
}

static int test_udp_exchange_matches_keepalive_phase(void) {
    int server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    struct timeval timeout;
    pid_t child;
    int status = 0;
    unsigned char request[6] = {0x07, 0x09, 0x28, 0x00, 0x0b, 0x01};
    unsigned char response[20] = {0};
    int result;

    if (server_fd < 0 || client_fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_port = 0;

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 ||
        getsockname(server_fd, (struct sockaddr *)&server_addr, &server_addr_len) < 0) {
        perror("bind/getsockname");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    child = fork();
    if (child < 0) {
        perror("fork");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    if (child == 0) {
        unsigned char received[6] = {0};
        unsigned char stale[20] = {0x07, 0x09, 0x28, 0x00, 0x0b, 0x04};
        unsigned char reply[20] = {0x07, 0x09, 0x28, 0x00, 0x0b, 0x02};
        struct sockaddr_in peer_addr;
        socklen_t peer_len = sizeof(peer_addr);

        close(client_fd);
        if (recvfrom(server_fd, received, sizeof(received), 0, (struct sockaddr *)&peer_addr, &peer_len) != (ssize_t)sizeof(received)) {
            _exit(2);
        }
        if (sendto(server_fd, stale, sizeof(stale), 0, (struct sockaddr *)&peer_addr, peer_len) != (ssize_t)sizeof(stale)) {
            _exit(3);
        }
        if (sendto(server_fd, reply, sizeof(reply), 0, (struct sockaddr *)&peer_addr, peer_len) != (ssize_t)sizeof(reply)) {
            _exit(4);
        }
        close(server_fd);
        _exit(0);
    }

    close(server_fd);
    result = drcom_udp_send_recv_expected_with_retries(client_fd, server_addr, request, sizeof(request), response, sizeof(response), "[test sent] ", "[test recv] ", 0x07, 0x09, 0x28, DRCOM_UDP_EXPECT_ANY, 0x02, sizeof(response));
    close(client_fd);

    if (waitpid(child, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (expect_true("udp helper skips stale keepalive phase", result == (int)sizeof(response)) != 0 ||
        expect_true("udp helper returned matching keepalive phase", response[0] == 0x07 && response[1] == 0x09 && response[5] == 0x02) != 0 ||
        expect_true("udp helper keepalive phase child succeeded", WIFEXITED(status) && WEXITSTATUS(status) == 0) != 0) {
        return 1;
    }

    return 0;
}

int main(void) {
    memset(&drcom_config, 0, sizeof(drcom_config));
    drcom_config.KEEP_ALIVE_VERSION[0] = 0xdc;
    drcom_config.KEEP_ALIVE_VERSION[1] = 0x02;

    if (expect_keepalive_version(DRCOM_KEEPALIVE_EXTRA_FIRST, 0x0f, 0x27) != 0 ||
        expect_keepalive_version(DRCOM_KEEPALIVE_EXTRA_PERIODIC, 0x0f, 0x27) != 0 ||
        expect_keepalive_version(DRCOM_KEEPALIVE_EXTRA_NONE, 0xdc, 0x02) != 0) {
        return 1;
    }

    if (test_udp_exchange_retries_after_dropped_reply() != 0) {
        return 1;
    }

    if (test_udp_exchange_skips_unexpected_packet() != 0) {
        return 1;
    }
    if (test_udp_exchange_keeps_destination_after_unexpected_source() != 0) {
        return 1;
    }
    if (test_udp_exchange_skips_stale_same_type_packet() != 0) {
        return 1;
    }
    if (test_udp_recv_skips_unexpected_without_resend() != 0) {
        return 1;
    }
    if (test_udp_recv_drains_many_unexpected_packets_until_expected_reply() != 0) {
        return 1;
    }
    if (test_udp_exchange_matches_challenge_random_echo() != 0) {
        return 1;
    }
    if (test_udp_exchange_matches_keepalive_phase() != 0) {
        return 1;
    }

    printf("keepalive packet smoke tests passed\n");
    return 0;
}
