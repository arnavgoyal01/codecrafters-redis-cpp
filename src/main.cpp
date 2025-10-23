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
	std::string dir = "";
	std::string dbfilename = ""; 

	int i = 1; 

	while (i < argc)
	{
		if ((std::string) argv[i] == "--port") port = std::stoi(argv[2]);
    if ((std::string) argv[i] == "--replicaof") role = "slave";	
		if ((std::string) argv[i] == "--dir") dir = argv[i+1];
		if ((std::string) argv[i] == "--dbfilename") dbfilename = argv[i+1];
		i += 2;
	}
	
	Server server = Server(port, role, dir, dbfilename);
	if (argc > 3 && (std::string) argv[3] == "--replicaof")
	{
		std::string input = argv[4];
		server.replicatingMaster(input);
	}

	server.loop(); 
  return 0;
}

