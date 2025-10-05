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

void getStuff(int); 

int main(int argc, char **argv) 
{
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) 
	{
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Since the tester restarts your program quite often,
	// setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse,
								 sizeof(reuse)) < 0) 
	{
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr,
					 sizeof(server_addr)) != 0) 
	{
    std::cerr << "Failed to bind to port 6379\n";
    return 1;
  }
  
	int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) 
	{
    std::cerr << "listen failed\n";
    return 1;
  }
  
	fd_set masterfds, writefds;
	int maxfd;
	std::vector<int> clientfds; 		
	std::cout << "Waiting for a client to connect...\n";
	int c = 0; 
	std::map<std::string, std::string> dict;	
	std::map<std::string,std::chrono::system_clock::time_point> times; 

	while (true)
	{
		FD_ZERO(&masterfds); 
		FD_SET(server_fd, &masterfds);
		maxfd = server_fd; 	
		for (auto cfd : clientfds)
		{	
			FD_SET(cfd, &masterfds);
			if (cfd > maxfd) maxfd = cfd; 
		}	
		
		int activity = select(maxfd + 1, &masterfds, NULL, NULL, NULL); 
		if (activity < 0) 
		{
			std::cerr << "select error\n"; 
			std::cout << "Error code: " <<  errno << " Num of client fds " << clientfds.size() << "\n"; 
			for (auto cfd : clientfds) std::cout << cfd;
			break; 
		}
		
		if (FD_ISSET(server_fd, &masterfds))
		{
			struct sockaddr_in client_addr;
			int client_addr_len = sizeof(client_addr);
			int client_fd1 = accept(server_fd,
													 (struct sockaddr *) &client_addr,
														(socklen_t *) &client_addr_len);
			
			std::cout << "Client fd1: " <<  client_fd1 << "\n"; 	
			if (client_fd1 < 0) 
			{
				std::cerr << "client 1 accept failed\n";
				continue;
			}
			clientfds.push_back(client_fd1);	
			std::cout << "Added new client to set "<< client_fd1 << "\n";
		}
 
		char buffer[256];
		bzero(buffer, 256);
		std::string response;
		std::string input;
		int n; 
		int i = 0; 

		while (i < clientfds.size())
		{ 
			if (FD_ISSET(clientfds[i], &masterfds))
			{
				int num_bytes = recv(clientfds[i], buffer, sizeof(buffer) - 1, 0);
				if (num_bytes == 0)
				{
					close(clientfds[i]);
					std::cout << "Disconnected " << clientfds[i] << "\n"; 
					clientfds.erase(clientfds.begin() + i); 
					i--;
					continue;
				}
				input = buffer; 
				std::vector<std::string> tokens; 
				size_t start = 0; 
				size_t end = input.find("$",start); 
				while (end != std::string::npos)
				{
					tokens.push_back(input.substr(start,end - start));
					start = end + 1; 
					end = input.find("$", start); 
				}
				tokens.push_back(input.substr(start)); 
				
				if (tokens[1] == "4\r\nPING\r\n")
				{
					response = "+PONG\r\n";	
				} 
				else if (tokens[1] == "4\r\nECHO\r\n") 
				{
					response = "$" + tokens[2];
				} 
				else if (tokens[1] == "3\r\nSET\r\n")
				{
					std::pair<std::string, std::string> pairi =
						{ tokens[2], tokens[3] };
					
					auto ret = dict.insert(pairi);
					
					if (ret.second == false)
					{
						std::cout << "Element already exists\n"; 
					}
					
					if (tokens.size() > 4)
					{
						size_t p1 = tokens[5].find("\r\n",0);
						int length = std::stoi(tokens[5].substr(0,p1));
						int duration = std::stoi(tokens[5].substr(p1 + 2, length)); 
						if (tokens[4] == "2\r\nEX\r\n") duration *= 1000;

						std::chrono::milliseconds t(duration); 

						std::chrono::system_clock::time_point time_limit
							= std::chrono::system_clock::now() + t;

						times.insert(std::pair<std::string,std::chrono::system_clock::time_point>(tokens[2],time_limit));
					}

					response = "+OK\r\n"; 
				} 
				else
				{
					response = "$" + dict[tokens[2]];
					auto it = times.find(tokens[2]); 
					if (it != times.end())
					{
						auto now = std::chrono::system_clock::now(); 
						if (it->second <= now)
						{
							std::cout << "Able to detect expiry\n";
							dict.erase(tokens[2]);
							times.erase(it); 
							response = "$-1\r\n";
						}
					}
				}
						
				int m = send(clientfds[i],
									 response.c_str(),
									 response.size(), 0);
				if (m < 0) 
				{
					std::cerr << "Error in send\n"; 
					std::printf("Socket error code %d\n", errno); 
				}
			} 
			i++;
		}
		
	}

	std::cout << "Passed through\n";
	clientfds.clear();
	close(server_fd);

  return 0;
}

