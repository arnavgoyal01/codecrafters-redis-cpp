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
#include "../headers/Server.h"

int main(int argc, char **argv) 
{
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
	
	int port = 6379; 
	if (argc > 1) port = std::stoi(argv[2]); 

	Server server = Server(port);
  
	server.loop(); 
  return 0;
}

