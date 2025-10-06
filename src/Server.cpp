#include "../headers/Server.h"
#include <string>
#include <utility>
#include <vector>

Server::Server()
{
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) 
	{
   std::cerr << "Failed to create server socket\n";
  }
  
  // Since the tester restarts your program quite often,
	// setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse,
								 sizeof(reuse)) < 0) 
	{
    std::cerr << "setsockopt failed\n";
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr,
					 sizeof(server_addr)) != 0) 
	{
    std::cerr << "Failed to bind to port 6379\n";
  }
  
	int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) 
	{
    std::cerr << "listen failed\n";  
  }
}

int Server::getFD()
{
	return server_fd;
}

int Server::reInit()
{
	FD_ZERO(&masterfds); 
	FD_SET(server_fd, &masterfds);
	maxfd = server_fd; 	
	for (auto cfd : clientfds)
	{	
		FD_SET(cfd, &masterfds);
		if (cfd > maxfd) maxfd = cfd; 
	}
	int activity =
		select(maxfd + 1, &masterfds, NULL, NULL, NULL); 
	
	if (activity < 0) 
	{
		std::cerr << "select error\n"; 
		std::cout << "Error code: " <<
			errno << " Num of client fds " <<
			clientfds.size() << "\n";  
	}
	return activity; 
}

void Server::getClients()
{
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
		}
		clientfds.push_back(client_fd1);	
		std::cout << "Added new client to set "
			<< client_fd1 << "\n";
	}
}

void Server::setValue()
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

void Server::getValue()
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

void Server::listPush()
{
	int n;
	int i = 1; 
	while (2 + i < tokens.size())
	{
		auto it = lists.find(tokens[2]); 
		if (it != lists.end())
		{
			it->second.push_back(tokens[2 + i]); 
			n = it->second.size();
		}
		else
		{
			lists.insert(
				std::pair<std::string, std::vector<std::string>>
				(tokens[2], std::vector<std::string>{ tokens[2 + i] })
			);
			n = 1; 
		}
		i++; 
	}
	response = ":" + std::to_string(n) + "\r\n";
}

void Server::listPushLeft()
{
	int n;
	int i = 1; 
	while (2 + i < tokens.size())
	{
		auto it = lists.find(tokens[2]); 
		if (it != lists.end())
		{
			it->second.insert(it->second.begin(), tokens[2 + i]); 
			n = it->second.size();
		}
		else
		{
			lists.insert(
				std::pair<std::string, std::vector<std::string>>
				(tokens[2], std::vector<std::string>{ tokens[2 + i] })
			);
			n = 1; 
		}
		i++; 
	}
	response = ":" + std::to_string(n) + "\r\n";
}

void Server::lrange()
{
	auto it = lists.find(tokens[2]); 
	if (it != lists.end())
	{
		int st = tokens[3].find("\r\n",0) + 2; 
		int ed = tokens[3].find("\r\n", st); 
		int start = std::stoi(tokens[3].substr(st, ed - st)); 
		
		st = tokens[4].find("\r\n",0) + 2; 
		ed = tokens[4].find("\r\n", st); 
		int stop = std::stoi(tokens[4].substr(st, ed - st));

		if (start < 0) start += it->second.size();
		if (stop < 0) stop += it->second.size(); 

		if (start < 0) start = 0;
		if (stop < 0) stop = 0; 

		if (start >= it->second.size())
		{
			response = "*0\r\n"; 
		}
		else 
		{
			if (stop >= it->second.size()) stop= it->second.size()-1;			
			if (start > stop) response = "*0\r\n"; 
			else 
			{
				response = "*" + std::to_string(stop-start+1) + "\r\n";
				for (int i = start; i <= stop; i++)
				{
					response += "$" + it->second[i];
				}
			}
		}
	}
	else 
	{
		response = "*0\r\n"; 
	}
}

void Server::getLength()
{
	auto it = lists.find(tokens[2]); 
	if (it != lists.end())
	{
		response = ":"
			+ std::to_string(it->second.size())
			+ "\r\n";
	}
	else 
	{
		response = ":0\r\n"; 
	}
}

void Server::LPOP()
{
	auto it = lists.find(tokens[2]); 
	if (it != lists.end())
	{
		if (it->second.size() != 0)
		{
			response = "$" + it->second[0]; 
			it->second.erase(it->second.begin(),
										it->second.begin() + 1); 
		}
		else 
		{
			response = "$-1\r\n";		
		}
	}
	else 
	{
		response = "$-1\r\n"; 
	}
}

void Server::commandCenter()
{
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
		setValue(); 		
	} 
	else if (tokens[1] == "3\r\nGET\r\n")
	{
		getValue(); 	
	} 
	else if (tokens[1] == "5\r\nRPUSH\r\n")
	{
		listPush();
	}
	else if (tokens[1] == "6\r\nLRANGE\r\n")
	{
		lrange(); 
	} else if (tokens[1] == "5\r\nLPUSH\r\n")
	{
		listPushLeft();
	} else if (tokens[1] == "4\r\nLLEN\r\n")
	{
		getLength();
	} else if (tokens[1] == "4\r\nLPOP\r\n")
	{
		LPOP();
	}


}

void Server::getInput()
{		
	bzero(buffer, 256);

	int i = 0; 

	while (i < clientfds.size())
	{ 
		if (FD_ISSET(clientfds[i], &masterfds))
		{
			int num_bytes =
				recv(clientfds[i], buffer, sizeof(buffer) - 1, 0);
			
			if (num_bytes == 0)
			{
				close(clientfds[i]);
				std::cout << "Disconnected " << clientfds[i] << "\n"; 
				clientfds.erase(clientfds.begin() + i); 
				i--;
				continue;
			}

			input = buffer; 
			tokens.clear();
			size_t start = 0; 
			size_t end = input.find("$",start); 
			while (end != std::string::npos)
			{
				tokens.push_back(input.substr(start,end - start));
				start = end + 1; 
				end = input.find("$", start); 
			}
			tokens.push_back(input.substr(start)); 
			
			commandCenter(); 
								
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

void Server::loop()
{
	while (true)
	{		
		if (reInit() < 0) break; 
		getClients(); 
		getInput();	
	}
}

Server::~Server()
{
	std::cout << "Passed through\n";
	clientfds.clear();
	close(server_fd);
}
