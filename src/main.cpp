#include <algorithm>
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
#include <vector>
#include <errno.h>

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
		std::string response = "+PONG\r\n";
		std::string input;
		int n; 
		int i = 0; 

		while (i < clientfds.size())
		{ 
			if (FD_ISSET(clientfds[i], &masterfds))
			{
				int num_bytes = recv(clientfds[i], buffer, sizeof(buffer) - 1, 0);
				std::cout << num_bytes << "\n"; 
				int m = send(clientfds[i],
									 response.c_str(),
									 response.size(), 0);
				if (m < 0) 
				{
					std::cerr << "Error in send\n"; 
					std::printf("Socket error code %d\n", errno); 
				}
				if (num_bytes == 0)
				{
					close(clientfds[i]);
					clientfds.erase(clientfds.begin() + i); 
					i--; 
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

void getStuff(int client_fd)
{

	char buffer[256];
	int n = 1; 
	while (n > 0)
	{
		bzero(buffer, 256);
		int n = recv(client_fd, buffer, 256, 0);
		if (n < 0) std::cerr << "Error reading input" << std::endl;
		std::string response = "+PONG\r\n";
		std::string input = buffer;

		for (int i = 0; i < input.size(); i++)
		{
			//send(client_fd, response.c_str(), response.size(),0);	
			if (input[i] == 'P')
			{	
				send(client_fd, response.c_str(), response.size(),0);
			}
		}
		if (n < 0) std::cerr << "Error sending response" << std::endl; 
	}
	close(client_fd);
}
