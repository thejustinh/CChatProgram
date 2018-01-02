/*****************************************************************************
* tcp_server.c
*
* CPE 464 - Program 1
*****************************************************************************/

#include "networks.h"
#include "flags.h"

#define DEBUG_FLAG 1

int recvFromClient(int clientSocket, struct handles * myhandles, int tsize);
int checkArgs(int argc, char *argv[]);


void sendACK(int socketNum, u_char * pkt, int pkt_len, int flag);
int sendFlag2_3(int clientSocket, struct handles * myhandles, int tsize);
void sendFlag7(char * handle, char * destHandle, struct handles * myhandles, int tsize);
void sendFlag9(int socketNum);
void sendHandles(int clientSocket, struct handles * myhandles, int tsize);
void sendFlag11(int clientSocket, struct handles * myhandles, int tsize);
void sendFlag12(int clientSocket, struct handles * myhandles, int tsize);
void sendFlag13(int clientSocket);


void handleConnections(int serverSocket);
int inTable(char * handle, struct handles * myhandles, int tsize);
void addToTable(int clientSocket, char * handle, struct handles * myhandles, int tsize);
void sendACK(int clientSocket, u_char * pkt, int pkt_len, int flag);
void forwardMessage(u_char * pkt, struct handles * myhandles, int tsize, int messageLen);
int getMessage(u_char *pkt);
void removeHandle(struct handles * myhandles, int tsize, int clientSocket);
int countHandles(struct handles * myhandles, int tsize);



int main(int argc, char *argv[])
{
	int serverSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);	

	//create the server socket
	serverSocket = tcpServerSetup(portNumber);

   handleConnections(serverSocket);

	return 0;
}

void sendACK(int socketNum, u_char * pkt, int pkt_len, int flag)
{

   if (send(socketNum, pkt, pkt_len, flag) < 0)
   {
      perror("send call");
      exit(-1);
   }
}

// Method to receive flag 1 packet and send either flag 2 or 3
int sendFlag2_3(int clientSocket, struct handles * myhandles, int tsize)
{
   char packet[MAX_PACKET_LEN];
   int len = 0;
   struct chathdr chdr;
   u_char pkt[CHAT_HDR_LEN];

   memset(packet, 0, MAX_PACKET_LEN);

   // receive flag 1 packet
   if ((len = recv(clientSocket, packet, MAX_PACKET_LEN, 0)) <= 0)
   {
      if (len == 0)
         printf("connection closed");
      else
         perror("recv call");
      return 0;
   }

   if (inTable(packet + 4, myhandles, tsize)) {// handle is already in use
      printf("In table");
      chdr.flag = FLAG3; // confirming good handle (1 byte)  
   } else {
      addToTable(clientSocket, packet + 4, myhandles, tsize);
      chdr.flag = FLAG2;
   }

   chdr.pkt_len = htons(sizeof(struct chathdr));
   memcpy(pkt, &chdr, CHAT_HDR_LEN);
   sendACK(clientSocket, pkt, CHAT_HDR_LEN, 0);

   return 1;
}


void sendFlag7(char * handle, char * destHandle, struct handles * myhandles, int tsize)
{
   int destHandleLen = strlen(destHandle);
   uint16_t messageLen = CHAT_HDR_LEN + 1 + destHandleLen;
   u_char pkt[messageLen];
   int clientSocket = inTable(handle, myhandles, tsize);

   memset(pkt, 0, messageLen);

   messageLen = htons(messageLen);
   memcpy(pkt, &messageLen, (int)sizeof(messageLen));
   pkt[CHAT_HDR_LEN - 1] = FLAG7; // save flag field in packet
   pkt[CHAT_HDR_LEN] = destHandleLen;
   memcpy(pkt + CHAT_HDR_LEN + 1, destHandle, destHandleLen);

   sendACK(clientSocket, pkt, ntohs(messageLen), 0);
}

void sendFlag9(int clientSocket)
{
   struct chathdr chdr;
   u_char pkt[CHAT_HDR_LEN];

   chdr.pkt_len = htons(CHAT_HDR_LEN);
   chdr.flag = FLAG9;
   memcpy(pkt, &chdr, CHAT_HDR_LEN);

   if (send(clientSocket, pkt, CHAT_HDR_LEN, 0) < 0)
   {
      perror("send call");
      exit(-1);
   }
}

void sendFlag11(int clientSocket, struct handles * myhandles, int tsize)
{
   uint32_t num_handles = htons(countHandles(myhandles, tsize));
   u_char pkt[CHAT_HDR_LEN + 4];
   struct chathdr chdr;

   chdr.pkt_len = htons(CHAT_HDR_LEN + 4);
   chdr.flag = FLAG11;

   memcpy(pkt, &chdr, CHAT_HDR_LEN);
   memcpy(pkt + CHAT_HDR_LEN, &num_handles, 4);

   if (send(clientSocket, pkt, CHAT_HDR_LEN + 4, 0) < 0)
   {
      perror("send call");
      exit(-1);
   }
}

void sendFlag12(int clientSocket, struct handles * myhandles, int tsize)
{
   int i = 0;
   int clientLen = 0;
   uint16_t pkt_len;
   u_char pkt[MAX_PACKET_LEN];

   for(i = 0; i < tsize; i++)
   {
      if (myhandles[i].flag != '\0')
      {
         clientLen = strlen(myhandles[i].handle);
         pkt_len = htons(CHAT_HDR_LEN + 1 + clientLen);

         memset(pkt, 0, MAX_PACKET_LEN);
         memcpy(pkt, &pkt_len, CHAT_HDR_LEN - 1);
         pkt[CHAT_HDR_LEN - 1] = FLAG12;
         pkt[CHAT_HDR_LEN] =  clientLen;
         memcpy(pkt + CHAT_HDR_LEN + 1, myhandles[i].handle, clientLen);

         if (send(clientSocket, pkt, ntohs(pkt_len), 0) < 0)
         {
            perror("send call");
            exit(-1);
         }
      }
   }
}

void sendFlag13(int clientSocket)
{
   struct chathdr chdr;
   u_char pkt[CHAT_HDR_LEN];

   chdr.pkt_len = htons(CHAT_HDR_LEN);
   chdr.flag = FLAG13;

   memcpy(pkt, &chdr, CHAT_HDR_LEN);

   if (send(clientSocket, pkt, CHAT_HDR_LEN, 0) < 0)
   {
      perror("send call");
      exit(-1);
   }
}

void sendHandles(int clientSocket, struct handles * myhandles, int tsize)
{
   sendFlag11(clientSocket, myhandles, tsize);
   sendFlag12(clientSocket, myhandles, tsize);
   sendFlag13(clientSocket);
}

// 0 means client disconnected
int recvFromClient(int clientSocket, struct handles * myhandles, int tsize)
{
	u_char pkt[MAX_PACKET_LEN];
   struct chathdr *chdr = NULL;
   int num_bytes = 0;
	uint16_t messageLen;
	
   memset(pkt, 0, MAX_PACKET_LEN);

	//now get the data from the client_socket
	if ((num_bytes = recv(clientSocket, pkt, 2, MSG_WAITALL)) <= 0)
	{
      if (num_bytes == 0) {
         return 0;
      } else {
		   exit(-1);
      }
      return 0;
	}

   chdr = (struct chathdr *)pkt;
   
   messageLen = ntohs(chdr->pkt_len) - 2;

   if ((num_bytes = recv(clientSocket, pkt + 2, messageLen, MSG_WAITALL)) < 0)
   {
      perror("recv call");
      exit(-1);
   }

   chdr = (struct chathdr *)pkt;

   if (chdr->flag == FLAG5)
      forwardMessage(pkt, myhandles, tsize, messageLen);
   if (chdr->flag == FLAG8) { 
      sendFlag9(clientSocket);
      return 0; // teardown client
   }
   if (chdr->flag == FLAG10)
      sendHandles(clientSocket, myhandles, tsize);

   return 1;
}

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}

void handleConnections(int serverSocket)
{
   int clientSocket = 0; // socket descript for the client socket
   int nfds = 0, i = 0;
   int tsize = 10; // initial table size
   struct handles * myhandles = malloc(tsize * (int)sizeof(struct handles));

   fd_set set; // set
   fd_set t_set; // temp set

   FD_ZERO(&set);
   FD_ZERO(&t_set);

   FD_SET(serverSocket, &set);
   nfds = serverSocket; // largest file descriptor (only 1 so far)

   while (1)
   {
      t_set = set; // repopulate temp set with sockets of interest

      if (select(nfds + 1, &t_set, NULL, NULL, NULL) < 0)
      {
         perror("error on select()\n");
         exit(-1);
      }

      for(i = 0; i <= nfds; i++)
      {
         if (FD_ISSET(i, &t_set))
         {
            if (i == serverSocket) {
               clientSocket = tcpAccept(serverSocket, DEBUG_FLAG);

               if (clientSocket == tsize) {
                  tsize = tsize + 10;
                  myhandles = realloc(myhandles, tsize * sizeof(struct handles));
               }

               if (sendFlag2_3(clientSocket, myhandles, tsize) == 0) { // Flag 3
                  removeHandle(myhandles, tsize, i);
                  close(i);
                  FD_CLR(i, &set);
               } else { // Flag 2 - Good handle
                  FD_SET(clientSocket, &set);
               }

               if (nfds < clientSocket) nfds = clientSocket;

            } else {
               if (recvFromClient(i, myhandles, tsize) == 0) {
                  removeHandle(myhandles, tsize, i);
                  close(i);
                  FD_CLR(i, &set);
               }
            }
         }
      }
   }

   // close connection to socket
   close(clientSocket);
}

void forwardMessage(u_char * pkt, struct handles * myhandles, int tsize, int messageLen)
{
   int offset = CHAT_HDR_LEN;
   int clientLen = pkt[offset]; // length of sending clients handle
   int destLen = 0;
   int num_dest = 0;
   int clientSocket = 0; // socket number of receiving client
   char destHandle[MAX_HANDLE_LEN]; // clients handle
   char clientHandle[clientLen + 1];

   memset(clientHandle, 0, clientLen + 1);
   offset = offset + clientLen + 1;

   num_dest = pkt[offset];

   strncpy(clientHandle, (char *)pkt + 4, clientLen);

   offset = offset + 1;
   while (num_dest > 0) {
      memset(destHandle, 0, MAX_HANDLE_LEN);
      destLen = pkt[offset]; // length of destination handle
      offset = offset + 1; // start of dest handle;
      strncpy(destHandle, (char *)(pkt + offset), destLen);

      if ((clientSocket = inTable(destHandle, myhandles, tsize)) > 0) { // valid destination
         sendACK(clientSocket, pkt, messageLen + 2, 0);
      } else {
         sendFlag7(clientHandle, destHandle, myhandles, tsize);
      }
      offset = offset + destLen;
      num_dest--;
   }
}



int getMessage(u_char * pkt)
{
   int offset = CHAT_HDR_LEN;
   int clientLen = pkt[offset];
   int destLen = 0;
   int num_dest = 0;

   offset = offset + clientLen + 1;

   num_dest = pkt[offset];

   offset = offset + 1;
   while (num_dest > 0) {
      destLen = pkt[offset];
      offset = offset + destLen + 1;
      num_dest--;
   }

   return offset;
}

void addToTable(int clientSocket, char * handle, struct handles * myhandles, int tsize)
{
   myhandles[clientSocket % tsize].flag = clientSocket;
   strcpy(myhandles[clientSocket % tsize].handle, handle);
}

void removeHandle(struct handles * myhandles, int tsize, int clientSocket)
{
   myhandles[clientSocket % tsize].flag = '\0';
   memset(myhandles[clientSocket % tsize].handle, 0, MAX_HANDLE_LEN);
}

int inTable(char * handle, struct handles * myhandles, int tsize)
{
   int i = 0;

   for (i = 0; i < tsize; i++)
   {
      if (myhandles[i].flag != '\0') {
         if (strcmp(myhandles[i].handle, handle) == 0)
            return myhandles[i].flag;
      }
   }
   return 0;
}

int countHandles(struct handles * myhandles, int tsize)
{
   int i, count = 0;

   for (i = 0; i < tsize; i++)
   {
      if (myhandles[i].flag != '\0')
         count += 1;
   }

   return count;
}




