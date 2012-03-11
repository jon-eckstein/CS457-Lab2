#include "../common/stepping_stone.h"
#include "../common/util.h"

char* port;

int main(int argc, char** argv)
{
	try
	{
		validateStartArguments(argc, argv);
	}
	catch(const char* message)
	{
	    displayErrorAndExit(message);
	}

	SteppingStone* ss = new SteppingStone(port);

	ss->start();

	return 1;
}

void validateStartArguments(int argc, char** argv)
{
	if(argc != 3)
	{
		throw "Invalid number of arguments";
	}

	char* option = *(argv + 1);

	if(strcmp(option, "-p") != 0)
	{
		throw "Invalid launch option";
	}

	port = *(argv + 2);

	uint32_t ssPort = atoi(port);

	if(ssPort <1024 || ssPort > 65535)
	{
		throw "Port must be in range of 1024 to 65535";
	}
}

void displayErrorAndExit(const char *message)
{
    error(message);
    error("usage: ss -p port");
    exit(1);
}


