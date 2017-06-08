#include <stdio.h> /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), bind(), sendto() and recvfrom() */
#include <sys/ioctl.h> 
#include <netinet/in.h> 
#include <net/if.h> 
#include <arpa/inet.h> /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h> /* for atoi() and exit() */
#include <string.h> /* for memset() */
#include <unistd.h> /* for close() */
#include <time.h>

#include  "packet.h"

#define MAX_SIZE 512 /* Longest string to echo */

