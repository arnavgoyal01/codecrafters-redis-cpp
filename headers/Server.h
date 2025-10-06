#pragma once 
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <sys/select.h>
#include <utility>
#include <vector>
#include <errno.h>
#include <map>
#include <chrono>


class Server 
{
private:
	
	int server_fd; 
	
	fd_set masterfds;
	
	int maxfd;
	
	std::vector<int> clientfds;

	char buffer[256];
	
	std::string response;
	
	std::string input;

	std::vector<std::string> tokens; 
			
	std::map<std::string, std::string> dict;	
	
	std::map<std::string,
	std::chrono::system_clock::time_point> times;

	std::map<std::string, 
	std::vector<std::string>> lists; 

public:

	Server();

	~Server(); 

	int getFD(); 

	int reInit(); 

	void getClients(); 

	void getInput();

	void commandCenter(); 

	void setValue(); 
	
	void getValue(); 

	void listPush();

	void listPushLeft();

	void lrange();

	void getLength();

	void LPOP();

	void loop(); 
};
