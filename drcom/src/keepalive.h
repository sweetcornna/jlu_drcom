#ifndef KEEPALIVE_H_
#define KEEPALIVE_H_

#define DRCOM_UDP_PACKET_RETRY_COUNT 3
#define DRCOM_UDP_EXPECT_ANY -1

int keepalive_1(int sockfd, struct sockaddr_in addr, unsigned char seed[], unsigned char auth_information[]);
int keepalive_2(int sockfd, struct sockaddr_in addr, int *keepalive_counter, int extra_packet_kind, int *encrypt_type);
void gen_crc(unsigned char seed[], int encrypt_type, unsigned char crc[]);
void keepalive_2_packetbuilder(unsigned char keepalive_2_packet[], int keepalive_counter, int extra_packet_kind, int type, int encrypt_type);
int drcom_udp_send_recv_with_retries(int sockfd,
                                     struct sockaddr_in addr,
                                     const unsigned char *send_packet,
                                     int send_length,
                                     unsigned char *recv_packet,
                                     int recv_length,
                                     char send_msg[10],
                                     char recv_msg[10]);
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
                                   int minimum_length);
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
                                              int minimum_length);

#endif  // KEEPALIVE_H_
