#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
bool nread(int fd, int len, uint8_t *buf) // finished function, no issues
{
  int total_read = 0, bytes_read;
  while (total_read < len)
  {
    bytes_read = read(fd, total_read + buf, len - total_read);
    if (bytes_read <= 0)
    {
      return false; // close connetion by returning false
    }
    total_read += bytes_read;
  }
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
bool nwrite(int fd, int len, uint8_t *buf) // finished, no issues
{
  int total_written = 0, bytes_written;
  while (total_written < len)
  {
    bytes_written = write(fd, buf + total_written, len - total_written);
    if (bytes_written <= 0)
    {
      return false;
    }
    total_written += bytes_written;
  }
  return true;
}
/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
bool recv_packet(int fd, uint32_t *op, uint8_t *ret, uint8_t *block)
{
  uint8_t header[HEADER_LEN];
  if (!nread(fd, HEADER_LEN, header))
  {
    return false; // returns false, error in reading header
  }
  memcpy(op, header, sizeof(uint32_t));
  *op = ntohl(*op); // Convert op to network byte order
  *ret = header[sizeof(uint32_t)];
  // determine if block needs to be read, 2nd bit of ret for a block
  bool read_block = (*ret & 0x02) != 0;

  if (read_block)
  {
    if (!nread(fd, JBOD_BLOCK_SIZE, block))
    {
      return false; // block could not be read
    }
  }

  return true;
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
bool send_packet(int sd, uint32_t op, uint8_t *block)
{
  uint8_t packet[HEADER_LEN + JBOD_BLOCK_SIZE];
  uint32_t op_net = htonl(op); // convert to network byte order
  memcpy(packet, &op_net, sizeof(op_net));

  // Determine if it's a jbod write command
  bool is_write_block = (op >> 12) == JBOD_WRITE_BLOCK; // bool is true if op shifted 12 bits to the right is equal to write command
  packet[sizeof(op_net)] = is_write_block ? 0x02 : 0x00;
  int packet_size = HEADER_LEN + (is_write_block ? JBOD_BLOCK_SIZE : 0); // caclulate packet size using header len and is_write_block as a condition
  if (is_write_block)                                                    // append data if the command is Jbod write
  {
    memcpy(packet + HEADER_LEN, block, JBOD_BLOCK_SIZE);
  }
  // Sends the packet
  return nwrite(sd, packet_size, packet);
}

/* connect to server and set the global client variable to the socket */
bool jbod_connect(const char *ip, uint16_t port) // finished function
{
  struct sockaddr_in servaddr;

  cli_sd = socket(AF_INET, SOCK_STREAM, 0);
  if (cli_sd < 0)
  {
    return false;
  }

  memset(&servaddr, 0, sizeof(servaddr)); // reset
  servaddr.sin_family = AF_INET;          // set to af_inet socket
  servaddr.sin_port = htons(port);        // convert to network byte order

  // convert to binary
  if (inet_pton(AF_INET, ip, &servaddr.sin_addr) <= 0)
  {
    return false;
  }

  if (connect(cli_sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
  {
    return false;
  }

  return true;
}

void jbod_disconnect(void) // finished function
{
  if (cli_sd >= 0)
  {
    close(cli_sd);
    cli_sd = -1;
  }
}

int jbod_client_operation(uint32_t op, uint8_t *block)
{
  extern int cli_sd;

  if (cli_sd == -1)
  {
    return -1;
  }
  // Send the operation packet to the server
  if (!send_packet(cli_sd, op, block))
  {
    return -1; // return -1 if error has occured
  }
  uint8_t resp_ret;
  // Receive the response packet from the server
  if (!recv_packet(cli_sd, &op, &resp_ret, block))
  {
    return -1;
  }
  return 0; // 0 for success
}
