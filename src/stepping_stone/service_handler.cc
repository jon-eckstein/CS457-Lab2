#include "../common/stepping_stone.h"
#include "../common/util.h"

fd_set masterSet;
fd_set readSet;
int maxFd = 0;
pthread_t handler[MAX_THREAD];
pthread_mutex_t threadMutex;
int threadIndex = 0;

void* startService(void* argument)
{
	StartArgument* startArg = (StartArgument*) argument;

	srand(time(NULL));

	int status = listen(startArg->listenerSocket, BACKLOG);

	if (status == FAILURE)
	{
		perror("Unable to listen on bind address");
		pthread_exit(NULL);
	}

	pthread_mutex_init(&threadMutex, NULL);

	selectConnection(startArg->listenerSocket);

	pthread_mutex_destroy(&threadMutex);

	pthread_exit(NULL);

	return NULL;
}

void selectConnection(int listenerSocket)
{
  initFileDescriptorSet(listenerSocket);

  struct timeval timeout;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;

  while(true)
  {
    readSet = masterSet;

    pthread_mutex_lock(&threadMutex);

    int status= select(maxFd + 1, &readSet, NULL, NULL, &timeout);

    pthread_mutex_unlock(&threadMutex);

    if(status == FAILURE)
    {
      perror("Unable to select");
      //pthread_exit(NULL);
    }
    else
    {
    	probeConnection(maxFd, listenerSocket);
    }
  }
}

void probeConnection(int maxFd, int listenerSocket)
{
   int i = 0;
   for(i=0;i<=maxFd;i++)
   {
      if(FD_ISSET(i, &readSet))
      {
    	 if(i == listenerSocket)
    	 {
 	        acceptConnectionAsync(listenerSocket);
    	 }
    	 else
    	 {
    		 receiveData(i);
    	 }
      }
   }
}

void acceptConnectionAsync(int listenerSocket)
{
    ConnectionRequest* connectionRequest = (ConnectionRequest*)malloc(sizeof(ConnectionRequest));
    connectionRequest->listenerSocket = listenerSocket;

    pthread_attr_t handlerAttribute;

    pthread_mutex_lock(&threadMutex);

    int index = 0;
    if (threadIndex < MAX_THREAD)
	{
		index = threadIndex++;
	}
	else
	{
		index = threadIndex = 0;
	}

    pthread_mutex_unlock(&threadMutex);

    pthread_attr_init(&handlerAttribute);
    pthread_attr_setdetachstate(&handlerAttribute, PTHREAD_CREATE_JOINABLE);
    pthread_create(&handler[index], NULL, &acceptConnection, (void*) connectionRequest);
}

void* acceptConnection(void* argument)
{
  sockaddr_in clientAddress;
  socklen_t   socketLength = sizeof(clientAddress);
  ConnectionRequest* connectionRequest = (ConnectionRequest*)argument;

  int connectionSocket = accept(connectionRequest->listenerSocket, (struct sockaddr*)&clientAddress, &socketLength);

  if(connectionSocket < 0)
  {
    perror("Error occurred while accepting connection from client");
    pthread_exit(NULL);
  }
  else
  {
    FD_SET(connectionSocket, &masterSet);
    if(connectionSocket > maxFd)
    {
      maxFd = connectionSocket;
    }
  }

  return NULL;
}

void receiveData(int socketid)
{
	AwgetRequest request;
	int status = receiveOnTCPSocket(socketid, &request, sizeof(request));

	if(status == SUCCESS)
	{
		debug("Request = URL %s \n", request.url);
		debug("ChainList size = %u \n", ntohs(request.chainListSize));
		info("SS list =[ \n");
		for(int i = 0;i< ntohs(request.chainListSize); i++)
		{
			SteppingStoneAddress ssAddress = request.chainList[i];
			debug("<%s>,<%u> \n", ssAddress.hostAddress, ntohl(ssAddress.port));
		}
		info("] \n");
	}

    handleRequestAsync(socketid, request);
}

void handleRequestAsync(int socketid, AwgetRequest request)
{
    pthread_attr_t handlerAttribute;

    TaskParameter* taskParameter = (TaskParameter*)malloc(sizeof(TaskParameter));
    taskParameter->socketid = socketid;
    taskParameter->awgetRequest = &request;

    pthread_mutex_lock(&threadMutex);

    int index = 0;
    if(threadIndex < MAX_THREAD)
	{
		index = threadIndex++;
	}
	else
	{
		index = threadIndex = 0;
	}

    pthread_mutex_unlock(&threadMutex);

    pthread_attr_init(&handlerAttribute);
    pthread_attr_setdetachstate(&handlerAttribute, PTHREAD_CREATE_JOINABLE);
    pthread_create(&handler[index], &handlerAttribute, &invokeFileRetriever, (void*)taskParameter);
}

void* invokeFileRetriever(void* argument)
{
	TaskParameter* taskParam = (TaskParameter*)argument;
	FileRetrieverService fileRetriever;

	try
	{
		if (ntohs(taskParam->awgetRequest->chainListSize) > 0)
		{
			info("waiting ... \n");

			fileRetriever.handleRequest(taskParam->awgetRequest,
					taskParam->socketid);
		}
		else
		{
			debug("This is the last SS, wget %s \n", taskParam->awgetRequest->url);

			fileRetriever.wget(taskParam->awgetRequest->url, taskParam->socketid);
		}
	}
	catch (char* errorMessage)
	{
		error(errorMessage);
	}

	info("Good bye . \n");

	pthread_mutex_lock(&threadMutex);

	close(taskParam->socketid);
    FD_CLR(taskParam->socketid, &masterSet);

    pthread_mutex_unlock(&threadMutex);

    return NULL;
}

int receiveOnTCPSocket(int socketid, AwgetRequest* request, size_t length)
{
	info("Receiving request... \n");

	recv(socketid, (void*)request, length, 0);

	return SUCCESS;
}

void initFileDescriptorSet(int socketid)
{
  maxFd = socketid;
  FD_ZERO(&masterSet);
  FD_ZERO(&readSet);
  FD_SET(socketid, &masterSet);
}
