/******************************************************************************
* tcp_client.c
*
*****************************************************************************/

#include "networks.h"
#include "flags.h"

#define DEBUG_FLAG 1
#define xstr(a) str(a)
#define str(a) #a

void mySelect(int socketNum, char * handle);
void checkArgs(int argc, char * argv[]);
int recvFromServer(int socketNum, struct handles * blocked);

void sendFlag1(int socketNum, char * handle);
void getFlag7(u_char * pkt);

void exitCommand(int socketNum);
void handleCommands(int socketNum, char * handle, struct handles * blocked, char * input);
void parseCommand(char * input, char * handle, int socketNum, struct handles * blocked);
void messageCommand(char *input,u_char *packet,char *handle, int socketNum);
void sendMessage(int socketNum, u_char * packet, char * msg);
void getMessage(u_char * pkt, struct handles * blocked);
void blockCommand(struct handles * blocked, char * input);
int isBlocked(struct handles * blocked, char * handle);
void addToBlocked(struct handles * blocked, char * handle);
void printBlocked(struct handles * blocked);
void unblockCommand(struct handles * blocked, char * input);
void unblock(struct handles * blocked, char * handle);
void listCommand(int socketNum);
void getHandles(u_char * pkt, int socketNum);


int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor

   /* Check arguments for Client Setup */
   checkArgs(argc, argv);

	/* Set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);

   /* Verify handle length and on server */
   sendFlag1(socketNum, argv[1]);

   mySelect(socketNum, argv[1]);
	return 0;
}


void mySelect(int socketNum, char * handle)
{
   int nfds = 0, i = 0;
   int tsize = 10;
   fd_set set;
   fd_set t_set;
   struct handles * blocked = malloc(tsize * (int)sizeof(struct handles));
   char input[MAX_PACKET_LEN];

   FD_ZERO(&set);
   FD_ZERO(&t_set);

   FD_SET(STDIN_FILENO, &set);
   FD_SET(socketNum, &set);
   nfds = socketNum;

   printf("$: ");
   fflush(stdout);

   while (1)
   {
      t_set = set;

      if (select(nfds + 1, &t_set, NULL, NULL, NULL) < 0)
      {
         perror("error on select()");
         exit(-1);
      }

      for(i = 0; i <= nfds; i++)
      {
         if (FD_ISSET(i, &t_set))
         {
            if (i == socketNum) {
               recvFromServer(socketNum, blocked);
            } else {
               fgets(input, MAX_PACKET_LEN, stdin);
               handleCommands(socketNum, handle, blocked, input);
            }
         }
      }
   }
}

int recvFromServer(int socketNum, struct handles * blocked)
{
   u_char pkt[MAX_PACKET_LEN];
   struct chathdr *chdr;
   int num_bytes = 0;
   uint16_t messageLen;

   memset(pkt, 0, MAX_PACKET_LEN);
   
   if ((num_bytes = recv(socketNum, pkt, 2, 0)) < 0)
   {
      perror("recv call");
      exit(-1);
   }

   chdr = (struct chathdr *)pkt;

   messageLen = ntohs(chdr->pkt_len) - 2;

   if ((num_bytes = recv(socketNum, pkt + 2, messageLen, 0)) < 0)
   {
      perror("recv call");
      exit(-1);
   }
   
   chdr = (struct chathdr *)pkt;

   if (chdr->flag == FLAG5)
      getMessage(pkt, blocked);

   if (chdr->flag == FLAG7)
      getFlag7(pkt);

   if (chdr->flag == FLAG9) {
      close(socketNum);
      exit(-1);
   }
   
   if (chdr->flag == FLAG11)
      getHandles(pkt, socketNum);
   return 1;
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s handle host-name port-number \n", argv[0]);
		exit(1);
	}
}

// Check to see if handle is valid
void sendFlag1(int socketNum, char * handle)
{
   struct chathdr chdr;
   u_char packet1[MAX_PACKET_LEN];
   u_char ack[CHAT_HDR_LEN];

   if (strlen(handle) > MAX_HANDLE_LEN - 1) {
      printf("Client handle too long. Must be <= 250 characters\n");
      close(socketNum);
      exit(-1);
   }

   // packet length should be in network order
   chdr.pkt_len = htons(CHAT_HDR_LEN + LEN_FIELD_LEN + strlen(handle));
   chdr.flag = 1;

   memset(packet1, 0, sizeof(packet1));

   memcpy(packet1, &chdr, CHAT_HDR_LEN); // copy chat header into packet
   packet1[CHAT_HDR_LEN] = strlen(handle); // copy length field (1 byte) into packet
   memcpy(packet1 + CHAT_HDR_LEN + LEN_FIELD_LEN, handle, strlen(handle)); // copy handle into packet

   // send the packet (excluding nulls and padding)
   if (send(socketNum, packet1, ntohs(chdr.pkt_len), 0) < 0) {
      perror("send call");
      exit(-1);
   }

   // block until recv 
   if (recv(socketNum, ack, CHAT_HDR_LEN, 0) < 0)
   {
      perror("recv call");
      exit(-1);
   }

   if (ack[2] == 3) {
      printf("handle already in use: %s\n", handle);
      close(socketNum);
      exit(-1);
   }
}

void getFlag7(u_char * pkt)
{
   int destHandleLen = pkt[CHAT_HDR_LEN];
   char destHandle[destHandleLen + 1];

   strncpy(destHandle, (char *)(pkt + CHAT_HDR_LEN + 1), destHandleLen);
   destHandle[destHandleLen] = '\0';

   printf("\nClient with handle %s does not exist.\n", destHandle);
   printf("$: ");
   fflush(stdout);
}

void exitCommand(int socketNum)
{
   struct chathdr chdr;
   u_char pkt[CHAT_HDR_LEN];

   chdr.pkt_len = htons(sizeof(struct chathdr));
   chdr.flag = FLAG8;
   memset(pkt, 0, CHAT_HDR_LEN);
   memcpy(pkt, &chdr, CHAT_HDR_LEN);

   if (send(socketNum, pkt, CHAT_HDR_LEN, 0) < 0)
   {
      perror("send call");
      exit(-1);
   }
}

void handleCommands(int socketNum, char * handle, struct handles * blocked, char * input)
{
   while (strlen(input) >= 1000 || strlen(input) == 1)
   {
      memset(input, 0, MAX_PACKET_LEN);
      printf("Invalid input. Please enter command <= 1000 and > 1 character\n");
      printf("$: ");
      fgets(input, MAX_PACKET_LEN, stdin);
   }

   parseCommand(input, handle, socketNum, blocked);
}

void parseCommand(char * input, char * handle, int socketNum, struct handles * blocked)
{
   u_char packet[MAX_PACKET_LEN];
   char input2[strlen(input) + 1]; // temp buffer for strtok
   strncpy(input2, input, strlen(input) + 1);
   char * word;

   word = strtok(input2, " \t\n");

   memset(packet, 0, MAX_PACKET_LEN);

   if (strcmp(word, "%M") == 0 || strcmp(word, "%m") == 0) {
      messageCommand(input, packet, handle, socketNum);
   } else if (strcmp(word, "%B") == 0 || strcmp(word, "\%b") == 0)
      blockCommand(blocked, input);
   else if (strcmp(word, "\%U") == 0 || strcmp(word, "\%u") == 0)
      unblockCommand(blocked, input);
   else if (strcmp(word, "%L") == 0 || strcmp(word, "\%l") == 0)
      listCommand(socketNum);
   else if (strcmp(word, "\%E") == 0 || strcmp(word, "\%e") == 0)
      exitCommand(socketNum);
   else {
      printf("Invalid command. Use %%m / %%b / %%u / %%l / %%e\n");
      printf("$: ");
      fflush(stdout);
      return;
   }
}

void messageCommand(char *input, u_char *packet, char *handle, int socketNum)
{
   char * word = strtok(input, " \t\n");
   int args = 0;
   int num_bytes = 0; // specifies index of next avaiable space in packet
   int num_handles = 1; // specifies number of handles to follow
   char msg[MAX_PACKET_LEN];
   int msg_bytes = 0;

   memset(msg, 0, MAX_PACKET_LEN);
   packet[CHAT_HDR_LEN - 1] = 5; // set flag field in packet to 5
   packet[CHAT_HDR_LEN] = strlen(handle); // save length of sending clients handle

   num_bytes = CHAT_HDR_LEN; // number of bytes so far is 3
   memcpy(packet + CHAT_HDR_LEN + LEN_FIELD_LEN, handle, strlen(handle)); // copy handle into packet 
   num_bytes = num_bytes + LEN_FIELD_LEN + strlen(handle);

   while ((word = strtok(NULL, " \t\n")) != NULL)
   {
      args++;
      if (atoi(word) != 0 && args == 1) { // if num-handles flag is set
         packet[num_bytes] = atoi(word);
         num_handles = atoi(word);
         if (num_handles > 9) {
            printf("Too many handle destinations. Please enter a number < 9\n");
            printf("$: ");
            fflush(stdout);
            return;
         }
         num_bytes = num_bytes + LEN_FIELD_LEN;
      } else if (atoi(word) == 0 && args == 1 && num_handles == 1) {
         packet[num_bytes] = num_handles;
         num_bytes = num_bytes + LEN_FIELD_LEN;
      }
      if (atoi(word) == 0 && num_handles > 0) { // if word is a destination handle
         if (strlen(word) >= MAX_HANDLE_LEN) {
            printf("Handle name is too long (Please enter handle <= 250 chars\n");
            printf("$: ");
            fflush(stdout);
            return;
         }
         packet[num_bytes] = strlen(word); // save destination handle length
         num_bytes = num_bytes + LEN_FIELD_LEN;
         memcpy(packet + num_bytes, word, strlen(word)); // save destinatino handle
         num_bytes = num_bytes + strlen(word);
         num_handles--;
      } else if (atoi(word) == 0 && num_handles == 0) {
         memcpy(msg + msg_bytes, word, strlen(word));
         msg_bytes = msg_bytes + strlen(word);
         msg[msg_bytes] = ' ';
         msg_bytes = msg_bytes + LEN_FIELD_LEN;
      }
   }

   if (args == 0) {
      printf("Please include destination handle\n");
      printf("usage$: [num-handles] destination-handle [destination-handle] [text]\n");
      printf("$: ");
      fflush(stdout);
      return;
   }

   sendMessage(socketNum, packet, msg);
   printf("$: ");
   fflush(stdout);
}


void sendMessage(int socketNum, u_char * packet, char * msg)
{
   u_char send_pkt[MAX_PACKET_LEN];
   uint16_t hdr_len;
   int offset = 0;

   while (strlen(msg + offset) > MAX_MSG_LEN) {
      memset(send_pkt, 0, MAX_PACKET_LEN);
      hdr_len = htons(2 + strlen((char *)(packet + 2)) + MAX_MSG_LEN);
      memcpy(send_pkt, &hdr_len, (int)sizeof(hdr_len)); // copy packet length
      memcpy(send_pkt + 2, packet + 2, strlen((char *)(packet + 2))); // copy packet handles
      memcpy(send_pkt + 2 + strlen((char *)(packet + 2)), msg + offset, MAX_MSG_LEN); // copy message      
      if (send(socketNum, send_pkt, ntohs(hdr_len), 0) < 0) {
         perror("send call");
         exit(-1);
      }

      offset = offset + MAX_MSG_LEN;
   }

   memset(send_pkt, 0, MAX_PACKET_LEN);
   hdr_len = htons(2 + strlen((char *)(packet + 2)) + strlen(msg + offset));

   send_pkt[0] = hdr_len & 0xff;
   send_pkt[1] = (hdr_len >> 8);

   memcpy(send_pkt + 2, packet + 2, strlen((char *)(packet + 2)));
   memcpy(send_pkt + 2 + strlen((char *)(packet + 2)), msg + offset, strlen(msg + offset));

   if (send(socketNum, send_pkt, ntohs(hdr_len), 0) < 0) {
      perror("send call");
      exit(-1);
   }
}

void getMessage(u_char * pkt, struct handles * blocked)
{
   int offset = CHAT_HDR_LEN;
   int clientLen = pkt[offset];
   int destLen = 0;
   int num_dest = 0;
   char msg[MAX_MSG_LEN + 1];
   char client[clientLen + 1];

   memset(client, 0, clientLen + 1);
   memset(msg, 0, MAX_MSG_LEN + 1);
   memcpy(client, pkt + CHAT_HDR_LEN + 1, clientLen);

   offset = offset + clientLen + 1;

   num_dest = pkt[offset];

   offset = offset + 1;
   while (num_dest > 0) {
      destLen = pkt[offset];
      offset = offset + destLen + 1;
      num_dest--;
   }

   memcpy(msg, pkt + offset, MAX_MSG_LEN);
   if (!isBlocked(blocked, client)) {
      printf("\n%s: %s\n", client, msg);
      printf("$: ");
      fflush(stdout);
   }
}

void blockCommand(struct handles * blocked, char * input)
{
   char * tok;
   int num_args = 0;

   if (strlen(input) > MAX_HANDLE_LEN) {
      printf("Handle too long. Please enter handle <= 250 characters\n");
      return;
   }

   tok = strtok(input, " \t\n");
   num_args++;

   while (tok != NULL && num_args < 2)
   {
      tok = strtok(NULL, " \t\n");
      num_args++;
   }

   if (num_args == 1 || tok == '\0') {
      printBlocked(blocked);
      printf("\n$: ");
      fflush(stdout);
      return;
   }

   if (!isBlocked(blocked, tok)) {
      addToBlocked(blocked, tok);
   } else
      printf("Block failed, handle %s already blocked.\n", tok);

   printBlocked(blocked);
   printf("\n$: ");
   fflush(stdout);
}

void addToBlocked(struct handles * blocked, char * handle)
{
   int i = 0;

   while(blocked[i].flag != '\0') i++;

   blocked[i].flag = i+1;
   strcpy(blocked[i].handle, handle);

   if (i % 9 == 0 && i != 0)
      blocked = realloc(blocked, (10 + i) * sizeof(struct handles));
}

int isBlocked(struct handles * blocked, char * handle)
{
   int i = 0;

   while(blocked[i].flag != '\0')
   {
      if (strcmp(blocked[i].handle, handle) == 0) {
         return 1;
      }
      i++;
   }

   return 0;
}

void printBlocked(struct handles * blocked)
{
   int i = 0;
   int first = 0;

   printf("Blocked:");

   while(blocked[i].flag != '\0')
   {
      if (*(blocked[i].handle) != '\0') {
         first++;
         if (first == 1)
            printf(" %s", blocked[i].handle);
         else
            printf(", %s", blocked[i].handle);
      }

      i++;
   }
}

void unblockCommand(struct handles * blocked, char * input)
{
   char * tok;
   int num_args = 0;

   if (strlen(input) > MAX_HANDLE_LEN) {
      printf("Handle too long. Please enter handle <= 250 characters\n");
      return;
   }

   tok = strtok(input, " \t\n");
   num_args++;

   while (tok != NULL && num_args < 2)
   {
      tok = strtok(NULL, " \t\n");
      num_args++;
   }

   if (num_args == 1 || tok == '\0') {
      printf("Unblock failed, no handle provided.\n");
      printf("$: ");
      fflush(stdout);
      return;
   }

   if (isBlocked(blocked, tok)) {
      unblock(blocked, tok);
      printf("Handle %s unblocked.\n", tok);
   } else {
      printf("Unblock failed, handle %s is not blocked\n", tok);
   }

   printf("$: ");
   fflush(stdout);
}

void unblock(struct handles * blocked, char * tok)
{
   int i = 0;

   while (blocked[i].flag != '\0')
   {
      if (strcmp(blocked[i].handle, tok) == 0)
         memset(blocked[i].handle, 0, strlen(tok));
      i++;
   }
}

void listCommand(int socketNum)
{
   struct chathdr chdr;
   u_char pkt[CHAT_HDR_LEN];

   chdr.pkt_len = htons(CHAT_HDR_LEN);
   chdr.flag = FLAG10;

   memcpy(pkt, &chdr, CHAT_HDR_LEN);

   if (send(socketNum, pkt, CHAT_HDR_LEN, 0) < 0)
   {
      perror("send call");
      exit(-1);
   }
}

void getHandles(u_char * pkt, int socketNum)
{
   uint32_t num_handles;
   struct chathdr *chdr;
   int messageLen = 0;
   int clientLen = 0;
   uint8_t init[3] = {1,2,3};
   char client[MAX_HANDLE_LEN];
   u_char pkt12[MAX_PACKET_LEN];

   memcpy(&num_handles, pkt + CHAT_HDR_LEN, (int)(sizeof(uint32_t)));
   num_handles = ntohs(num_handles);
   printf("Number of clients: %d\n", num_handles);

   chdr = (struct chathdr *)init;
   memset(chdr, 0, CHAT_HDR_LEN);
   memset(pkt12, 0, MAX_PACKET_LEN);

   while (chdr->flag != FLAG13)
   {
      if (recv(socketNum, pkt12, 2, 0) < 0) {
         perror("recv call");
         exit(-1);
      }
      chdr = (struct chathdr *)pkt12;
      messageLen = ntohs(chdr->pkt_len) - 2;

      if (recv(socketNum, pkt12 + 2, messageLen, 0) < 0) {
         perror("recv call");
         exit(-1);
      }

      chdr = (struct chathdr *)pkt12;

      if (chdr->flag == FLAG12) {
         memset(client, 0, MAX_HANDLE_LEN);
         clientLen = pkt12[CHAT_HDR_LEN];
         strncpy(client, (char *)(pkt12 + CHAT_HDR_LEN + 1), clientLen);
         printf("   %s\n", client);
      }
   }
   printf("$: ");
   fflush(stdout);
}






