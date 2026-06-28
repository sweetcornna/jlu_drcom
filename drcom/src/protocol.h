#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include "configparse.h"

#define DRCOM_DHCP_LOGOUT_PACKET_SIZE 80

unsigned int drcom_dhcp_login_packet_size(size_t password_length, int ror_version, int try_JLUversion);
void drcom_build_dhcp_logout_packet(const struct config *config,
                                    const unsigned char logout_seed[4],
                                    const unsigned char auth_information[16],
                                    unsigned char packet[DRCOM_DHCP_LOGOUT_PACKET_SIZE]);

#endif  // PROTOCOL_H_
