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
#include <queue>
#include <unordered_set>

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

	std::map<
		int, std::chrono::system_clock::time_point> blocklist;

	std::map<std::string, 
		std::queue<int>> queues;  

	std::map<std::string,
			std::map<std::string,
				std::map<std::string, std::string>*>> streams; 

public:

	Server();

	~Server(); 

	int getFD(); 

	int reInit(); 

	void getClients(); 

	bool getInput(int& i);

	void sendData(int& i, int type); 
	
	void sendData(int& i, int type, std::string r); 

	bool commandCenter(int cfd); 

	void setValue(); 
	
	void getValue(); 

	void listPush();

	void listPushLeft();

	void lrange();

	void getLength();

	void LPOP();
	
	bool BLPOP(int cfd);

	void TYPE();

	void XADD();

	void BLPOP_RESOLVE(std::string key);

	void resolveID();

	void controller();

	void loop(); 
};
