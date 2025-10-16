#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <sstream>
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
#include "../headers/Server.h"

int main(int argc, char **argv) 
{
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
	
	int port = 6379; 
	std::string role = "master";
	
	if (argc > 1) port = std::stoi(argv[2]); 
	if (argc > 3) role = "slave"; 
	Server server = Server(port, role);
	if (argc > 3 && (std::string) argv[3] == "--replicaof")
	{
		std::string input = argv[4];
		server.replicatingMaster(input);
	}

	server.loop(); 
  return 0;
}

