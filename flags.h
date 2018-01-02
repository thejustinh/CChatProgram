#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define MAX_PACKET_LEN 1003
#define MAX_HANDLE_LEN 251
#define MAX_MSG_LEN 200
#define CHAT_HDR_LEN 3
#define LEN_FIELD_LEN 1

#define FLAG1 1
#define FLAG2 2
#define FLAG3 3
#define FLAG5 5
#define FLAG7 7
#define FLAG8 8
#define FLAG9 9
#define FLAG10 10
#define FLAG11 11
#define FLAG12 12
#define FLAG13 13

struct chathdr {
   uint16_t pkt_len;
   uint8_t flag;
} __attribute((packed));

struct handles {
   int flag;
   char handle[MAX_HANDLE_LEN];
};

