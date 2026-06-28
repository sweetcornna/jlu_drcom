#include <stdio.h>
#include <string.h>

#include "../configparse.h"
#include "../protocol.h"

struct config drcom_config;

static int expect_true(const char *name, int condition) {
    if (!condition) {
        fprintf(stderr, "assertion failed: %s\n", name);
        return 1;
    }
    return 0;
}

int main(void) {
    unsigned char logout_seed[4] = {0x01, 0x02, 0x03, 0x04};
    unsigned char auth_information[16] = {
        0x10, 0x11, 0x12, 0x13,
        0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b,
        0x1c, 0x1d, 0x1e, 0x1f};
    unsigned char packet[DRCOM_DHCP_LOGOUT_PACKET_SIZE];

    memset(&drcom_config, 0, sizeof(drcom_config));

    if (expect_true("standard login packet size", drcom_dhcp_login_packet_size(9, 0, 0) == 330) != 0 ||
        expect_true("ror odd password packet size is padded", drcom_dhcp_login_packet_size(9, 1, 0) == 340) != 0 ||
        expect_true("ror even password packet size", drcom_dhcp_login_packet_size(10, 1, 0) == 340) != 0 ||
        expect_true("short ror password packet size", drcom_dhcp_login_packet_size(8, 1, 0) == 338) != 0 ||
        expect_true("JLU login packet size keeps template padding", drcom_dhcp_login_packet_size(16, 0, 1) == 374) != 0) {
        return 1;
    }

    snprintf(drcom_config.username, sizeof(drcom_config.username), "%s", "student");
    snprintf(drcom_config.password, sizeof(drcom_config.password), "%s", "secret");
    drcom_config.CONTROLCHECKSTATUS = 0x20;
    drcom_config.ADAPTERNUM = 0x05;
    drcom_config.mac[0] = 0xb0;
    drcom_config.mac[1] = 0x25;
    drcom_config.mac[2] = 0xaa;
    drcom_config.mac[3] = 0x85;
    drcom_config.mac[4] = 0x10;
    drcom_config.mac[5] = 0x14;

    memset(packet, 0xff, sizeof(packet));
    drcom_build_dhcp_logout_packet(&drcom_config, logout_seed, auth_information, packet);

    if (expect_true("logout code", packet[0] == 0x06) != 0 ||
        expect_true("logout type", packet[1] == 0x01) != 0 ||
        expect_true("logout eof", packet[2] == 0x00) != 0 ||
        expect_true("logout username length", packet[3] == strlen(drcom_config.username) + 20) != 0 ||
        expect_true("logout username copied", memcmp(packet + 20, drcom_config.username, strlen(drcom_config.username)) == 0) != 0 ||
        expect_true("control check copied", packet[56] == drcom_config.CONTROLCHECKSTATUS) != 0 ||
        expect_true("adapter num copied", packet[57] == drcom_config.ADAPTERNUM) != 0 ||
        expect_true("mac xor md5 byte 0", packet[58] == (unsigned char)(packet[4] ^ drcom_config.mac[0])) != 0 ||
        expect_true("mac xor md5 byte 5", packet[63] == (unsigned char)(packet[9] ^ drcom_config.mac[5])) != 0 ||
        expect_true("auth information copied", memcmp(packet + 64, auth_information, sizeof(auth_information)) == 0) != 0) {
        return 1;
    }

    printf("protocol packet smoke tests passed\n");
    return 0;
}
