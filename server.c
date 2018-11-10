#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <ctype.h>
#include <pthread.h>
#include <signal.h>

#define DEFAULT_PORT 8080
#define MAX_THREADS 2
#define MAX_CLIENTS 3

//Mutexes
pthread_mutex_t boundedBuffLock;
pthread_mutex_t logLock;

//Condition variables
pthread_cond_t boundedBuffFull;
pthread_cond_t boundedBuffEmpty;

//Bounded buffer to store clients
int boundedBuff[MAX_CLIENTS];
int numOfClients = 0; //Number of clients in bounded buff
int buffIndex = 0; //To keep track of where in bounded buff to store client
int removeClient = 0; //Keep track of where in bounded buff to remove client

//Dictionary
char ** dict;
int dictSize = 0;//Dictionary size

//File pointer for log file
FILE *logger;

//Functions
char** readDictToArr(char * fileName);
int checkDict(char * word, char ** dict);
void *processClient(void *arg);
void addClient(int socfd);
int getClient();
void logResult(char * word, int isWord);

int main(int argc, char * argv[]){
  //Open/Create log file
  logger = fopen("log.txt", "w");
  if(logger == NULL){
    perror("\nError opening log file.\n");
    exit(EXIT_FAILURE);
  }
  fclose(logger);
  
  int portNum;
  if(argc == 3){ //Port number and dictionary provided
    dict = readDictToArr(argv[2]);
    portNum = atoi(argv[1]);
  } else if(argc == 2){ //Port number entered, but no dictionary entered so use default dictionary
    dict = readDictToArr("words.txt");
    portNum = atoi(argv[1]);
  } else { //No port number or dictionary provided
    dict = readDictToArr("words.txt");
    portNum = DEFAULT_PORT;
  }

  /*Initializing mutexes and condition variables*/
  if(pthread_mutex_init(&boundedBuffLock, NULL) != 0){ //Failed to initialize bounded buff lock
    perror("Lock failed to initialize.\n");
  }
  if(pthread_cond_init(&boundedBuffFull, NULL) != 0){ //Failed to initialize condition variable
    perror("Condition variable failed to initialize.\n");
  }
  if(pthread_cond_init(&boundedBuffFull, NULL) != 0){ //Failed to initialize condition variable
    perror("Condition variable failed to initialize.\n");
  }
  if(pthread_mutex_init(&logLock, NULL) != 0){ //Failed to initialize log lock
    perror("Condition variable failed to initialize.\n");
  }

  /*Creating worker threads*/
  pthread_t workers[MAX_THREADS];

  /*Creating Socket*/
  int sockfd, newsockfd;

  struct sockaddr_in serverAddr, clientAddr;
  socklen_t clientLen;
  
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0){ //Socket function failed
    perror("Error opening socket");
    exit(1);
  }

  //Clear serverAddr
  bzero((char *) &serverAddr, sizeof(serverAddr));
  
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(portNum);
 
  /*Binding*/
  if(bind(sockfd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0){ //Binding failed
    perror("Binding failed\n");
    exit(1);
  }

  /*Initializing worker threads*/
  int i;
  for(i = 0; i < MAX_THREADS; i++){
    if(pthread_create(&workers[i], NULL, processClient, NULL) != 0){
      fprintf(stderr, "Error, creating thread %d.\n", i);
    }
  }

  /*Listening for Connection*/
  listen(sockfd, 3);
  clientLen = sizeof(clientAddr);

  /*Accepting Connections*/
  /*Receive incoming clients and store them in bounded buffer*/
  while(1){
    if(numOfClients < MAX_CLIENTS){ //Only accept connections if number of clients in bounded buffer
                                    //Has not exceeded max number of clients allowed in bounded buffer
      newsockfd = accept(sockfd, (struct sockaddr *) &clientAddr, &clientLen);
      if(newsockfd < 0){ //Failed to accept connection
	perror("Failed to accept connection\n");
	exit(1);
      } else {
	printf("Client accepted.\n");
      }
      //After accepting connection, put client in bounded buffer
      addClient(newsockfd);
    }
  }
  close(sockfd);
  return 0;
}

//Function to log results of spellchecker to log file
void logResult(char * word, int isWord){
  pthread_mutex_lock(&logLock); //Lock the log file
  logger = fopen("log.txt", "a"); //Open log file to append
  if(logger == NULL){
    perror("\nError opening log file.\n");
    exit(EXIT_FAILURE);
  }
  //Print result to log file
  if(isWord == 1){ //Yes is word
    fprintf(logger, "%s - OK. ", word);
  } else { //No is not word
    fprintf(logger, "%s - MISPELLED. ", word);
  }
  fclose(logger);
  pthread_mutex_unlock(&logLock); //Unlock the file
}

//Function to retrieve client from bounded buffer
int getClient(){
  pthread_mutex_lock(&boundedBuffLock); //Lock bounded buffer to get client from it
  while(numOfClients == 0){ //If bounded buffer is empty, wait for clients
    pthread_cond_wait(&boundedBuffFull, &boundedBuffLock);
  }
  int sockfd = boundedBuff[removeClient];
  numOfClients--; //Decrease number of clients in the bounded buff
  removeClient++; //Increment this so the next client removed is at the next index
  if(removeClient == MAX_CLIENTS){ //If reached end of bounded buff
    removeClient = 0; //Reset removeClient to start of buff
  }
  pthread_cond_signal(&boundedBuffEmpty); //Signal to threads that bounded buff has an empty slot
  pthread_mutex_unlock(&boundedBuffLock); //Unlock the bounded buff
  return sockfd;
}

void addClient(int socfd){
  pthread_mutex_lock(&boundedBuffLock); //Lock the bounded buffer to add client to it
  while(numOfClients == MAX_CLIENTS){ //If bounded buffer is full, wait for an empty slot
    pthread_cond_wait(&boundedBuffEmpty, &boundedBuffLock);
  }
  //Add client to bounded buffer
  boundedBuff[buffIndex] = socfd;
  numOfClients++; //Increment number of clients in buffer
  buffIndex++; //Next client will be stored at next index
  if(buffIndex == MAX_CLIENTS){ //Reset buffIndex to 0, if reached end of bounded buffer
    buffIndex = 0;
  }
  pthread_cond_signal(&boundedBuffFull); //Signal to threads waiting for client in bounded buffer
  pthread_mutex_unlock(&boundedBuffLock); //Unlock the bounded buffer
}

//Function to check if word is in dictionary
//Returns 0 for FALSE, 1 for TRUE
int checkDict(char * word, char ** dict){
  int i;
  int isWord = 0;
  for(i = 0; i < dictSize; i++){
    int r = strncmp(word, dict[i], strlen(word));
    if(r == 0){
      isWord = 1;
      return isWord;
    }
  }
  isWord = 0;
  return isWord;
}

//Function to read dictionary into an array of strings
char ** readDictToArr(char * fileName){
  char * line = NULL;
  char word[30];
  FILE * fp = fopen(fileName, "r");
  size_t len = 0;
  ssize_t read;

  if(fp == NULL){
    perror("\nError opening file.\n");
    exit(EXIT_FAILURE);
  }
  
  //Get number of words in dictionary
  while((read = getline(&line, &len, fp)) != -1){
    dictSize++;
  }
  fclose(fp);
  
  //Allocate memory to array
  char ** dict = (char**)malloc(dictSize*sizeof(char*));
  int i;
  //Allocate memory to each string in array
  for(i = 0; i < dictSize; i++){
    dict[i] = (char*)malloc(30*sizeof(char));
  }

  FILE * file = fopen("words.txt", "r");
  if(file == NULL){
    perror("\nError opening file.\n");
    exit(EXIT_FAILURE);
  }

  //Copy words from dictionary to array
  int m = 0;
  while(fgets(word, sizeof(word), file) != NULL){
    strcpy(dict[m], word);
    m++;
  }
  fclose(file);
  return dict;
  
}

/*Thread function*/
//Communicates between client and server
//Checks spelling
//And logs result to log file
void *processClient(void *arg){
  char buff[255];
  int n;
  int newsockfd;
  char prompt[100] = "Enter a word. Type \"exitpls\" to quit.\n";
  int isWord = 0;
  
  while(1){
    newsockfd = getClient(); //Get client from bounded buffer
    /*Communicating between client and server*/
    while(1){
      //Clear anything in word buffer
      bzero(buff, 255);
  
      isWord = 0; //Boolean for if word spelt correctly

      //Writing prompt to client
      n = write(newsockfd, prompt, strlen(prompt));
      if(n <0){ //Write failed
	perror("Failed to write to client\n");
	exit(1);
      }
    
      //Receive word from client which will be stored in buff
      n = read(newsockfd, buff, 255);

      if(n == 0){ //If keyboard interrupt entered ie. ctrl-c ctrl-c
	break; //Exit inner loop
      } else if(n < 0){ //Read failed
	perror("Failed to read from client\n");
	exit(1);
      }
      
      //If client enters exitpls, exit the loop
      int i = strncmp("exitpls", buff, 7);
      if(i == 0){
	break;
      }

      //Check dictionary
      isWord = checkDict(buff, dict);
      char word[30]; //To store user word
      if(isWord){
	strcpy(word, buff); 
	//Clear buff
	bzero(buff, 255);
	//Writing to client
	strcat(buff, word);
	strcat(buff, " - OK.\n");
	n = write(newsockfd, buff, strlen(buff));
	if(n <0){ //Write failed
	  perror("Failed to write to client\n");
	  exit(1);
	}
	logResult(word, isWord);
      } else {
        strcpy(word, buff);
	//Clear buff
	bzero(buff, 255);
	//Writing to client
	strcat(buff, word);
	strcat(buff, " - MISPELLED.\n");
	n = write(newsockfd, buff, strlen(buff));
	if(n <0){ //Write failed
	  perror("Failed to write to client\n");
	  exit(1);
	}
	logResult(word, isWord);
      }
    }
    //When inner loop is exited, terminate client connection
    close(newsockfd);
  }
  return 0;
}
