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

	std::unordered_set<int> mul; 

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
		std::vector<
			std::pair<
				std::string, int>>> stream_queues;  

	std::map<std::string,
			std::map<std::string,
				std::map<std::string, std::string>>> streams; 

	std::map<int, 
		std::queue<std::vector<std::string>>> queued_commands; 

public:

	Server(int port);

	~Server(); 

	int getFD(); 

	bool reInit(); 

	void getClients(); 

	bool getInput(int& i);
	
	void sendData(int& i, std::string r); 

	bool commandCenter(int cfd); 

	void setValue(); 
	
	void getValue(); 

	void INCR(); 

	void listPush();

	void listPushLeft();

	void lrange();

	void getLength();

	void LPOP();
	
	bool BLPOP(int cfd);

	void TYPE();

	void XADD();

	void XRANGE();	

	void XREAD(); 
	
	void XREAD_BLOCK(int cfd); 

	void BLPOP_RESOLVE(std::string key);

	void XREAD_BLOCK_RESOLVE(std::string key);

	void resolveID();

	void MULTI(int cfd); 

	void controller();

	void loop(); 
};
