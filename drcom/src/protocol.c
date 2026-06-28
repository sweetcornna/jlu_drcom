#include "protocol.h"

#include <string.h>

#include "libs/md5.h"

unsigned int drcom_dhcp_login_packet_size(size_t password_length, int ror_version, int try_JLUversion) {
    size_t length_padding = 0;
    int use_ror_version = ror_version || try_JLUversion;

    if (!use_ror_version) {
        return 330;
    }

    if (password_length > 8) {
        length_padding = password_length - 8;
        if (try_JLUversion) {
            size_t jlu_padding = password_length == 16 ? 0 : password_length / 4;
            length_padding = 28 + (password_length - 8) + jlu_padding;
        } else if (length_padding % 2 != 0) {
            length_padding++;
        }
    }

    return (unsigned int)(338 + length_padding);
}

void drcom_build_dhcp_logout_packet(const struct config *config,
                                    const unsigned char logout_seed[4],
                                    const unsigned char auth_information[16],
                                    unsigned char packet[DRCOM_DHCP_LOGOUT_PACKET_SIZE]) {
    unsigned char md5a[16];
    size_t password_length = strlen(config->password);
    size_t username_length = strlen(config->username);
    size_t md5a_input_length = 6 + password_length;
    unsigned char md5a_input[6 + sizeof(config->password)];

    memset(packet, 0, DRCOM_DHCP_LOGOUT_PACKET_SIZE);
    packet[0] = 0x06;
    packet[1] = 0x01;
    packet[2] = 0x00;
    packet[3] = (unsigned char)(username_length + 20);

    memset(md5a_input, 0, sizeof(md5a_input));
    md5a_input[0] = 0x06;
    md5a_input[1] = 0x01;
    memcpy(md5a_input + 2, logout_seed, 4);
    memcpy(md5a_input + 6, config->password, password_length);
    MD5(md5a_input, md5a_input_length, md5a);

    memcpy(packet + 4, md5a, sizeof(md5a));
    memcpy(packet + 20, config->username, username_length);
    packet[56] = config->CONTROLCHECKSTATUS;
    packet[57] = config->ADAPTERNUM;

    for (int index = 0; index < 6; index++) {
        packet[58 + index] = (unsigned char)(md5a[index] ^ config->mac[index]);
    }

    memcpy(packet + 64, auth_information, 16);
}
