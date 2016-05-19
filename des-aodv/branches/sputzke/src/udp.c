#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
//char * host_name = "127.0.0.1"; 
int socketDescriptor;

  int socketDescriptor;
  int serverPort = 50000;
  struct sockaddr_in serverAddress;
  double value;

/*int main() {
  getValue("127.0.0.1",50000,"130.73.63.62 1 5");
  printf("Return: %f\n",value);
}*/
int getValue (char* host_name,int serverPort, char* message){
  printf("main\n");
  serverAddress.sin_family = AF_INET;
  inet_aton(&host_name, &serverAddress.sin_addr);
  serverAddress.sin_port = htons(serverPort); 
  
  if ((socketDescriptor = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    printf("Could not create socket. \n");
    exit(1); // quits the program
  }
  printf("socket created: %i\n",socketDescriptor);
  
  int foo = sender(message);
  int bar = receiver();
  return 1;
}


int sender (char msg[])
{
  printf("serverport:%i\n",serverAddress.sin_port);
  //char msg[] = "130.73.63.62 1 5";
  //printf("We will send the message: \"%s\" to the server. \n", msg); 
  //printf("using socket %d\n",socketDescriptor);
  if (sendto(socketDescriptor, msg, strlen(msg), 0, (struct sockaddr *)
      &serverAddress, sizeof(serverAddress)) < 0) {
    printf("Could not send data to the server. \n");
    exit(1);
  }
  return 0;
}

int receiver() {
  
  int serverAddress_len;
  char message [256];
  struct hostent *server_host_name;
  server_host_name = gethostbyname("127.0.0.1"); 
  bzero(&serverAddress, sizeof(serverAddress));
  printf("Bind recv using socket %d\n",socketDescriptor);
  if (bind(socketDescriptor, (struct sockaddr *)&serverAddress, sizeof(serverAddress)))
  serverAddress_len = sizeof(serverAddress); 
   recvfrom(socketDescriptor, &message, sizeof(message), 0,
    (struct sockaddr *)&serverAddress, &serverAddress_len);
  printf("\nResponse from server:\n%s\n", message);
  value = atof (message); 
  close(socketDescriptor);
  return 1;
}


