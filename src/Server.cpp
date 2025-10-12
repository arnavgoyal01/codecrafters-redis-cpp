#include "../headers/Server.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iostream>
#include <map>
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

bool Server::reInit()
{
	struct timeval tv;
	FD_ZERO(&masterfds); 
	FD_SET(server_fd, &masterfds);
	maxfd = server_fd; 	
	for (auto cfd : clientfds)
	{
		FD_SET(cfd, &masterfds);
		if (cfd > maxfd) maxfd = cfd; 
	}
	tv.tv_sec = 0;
  tv.tv_usec = 0;
	int activity =
		select(maxfd + 1, &masterfds, NULL, NULL, &tv); 

	if (activity < 0) 
	{
		std::cerr << "select error\n"; 
		std::cout << "Error code: " <<
			errno << " Num of client fds " <<
			clientfds.size() << "\n";  
	}
	return (activity < 0); 
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
		
		// std::cout << "Client fd1: " <<  client_fd1 << "\n"; 	
		if (client_fd1 < 0) 
		{
			std::cerr << "client 1 accept failed\n";			
		}
		clientfds.push_back(client_fd1);	
		// std::cout << "Added new client to set " << client_fd1 << "\n";
	}
}

void Server::setValue()
{	
	std::pair<std::string, std::string> pairi =
		{ tokens[2], tokens[3] };
	
	auto ret =dict.insert(pairi);
	
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
	BLPOP_RESOLVE(tokens[2]); 
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

	BLPOP_RESOLVE(tokens[2]); 
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
		int i = 1; 
		response = ""; 
		if (tokens.size() > 3)
		{
			int st = tokens[3].find("\r\n",0) + 2; 
			int ed = tokens[3].find("\r\n", st); 
			i = std::stoi(tokens[3].substr(st, ed - st));	
			if (i > it->second.size()) i = it->second.size(); 
			response += "*" + std::to_string(i) + "\r\n";
		}

		for (int j = 0; j < i; j++)
		{
			response += "$" + it->second[j]; 
		}
		it->second.erase(it->second.begin(),
									it->second.begin() + i);
		if (it->second.size() == 0) lists.erase(it);	
	}
	else 
	{
		response = "$-1\r\n"; 
	}
}

bool Server::BLPOP(int cfd)
{
	auto it = lists.find(tokens[2]); 
	if (it != lists.end())
	{
		response = "*2\r\n$" + tokens[2] + "$" + it->second[0]; 	
		it->second.erase(it->second.begin(),
									 it->second.begin() + 1);
		if (it->second.size() == 0) lists.erase(it);
		return true; 
	}
	else 
	{	
		int st = tokens[3].find("\r\n",0) + 2; 
		int ed = tokens[3].find("\r\n", st); 
		float d = std::stof(tokens[3].substr(st, ed - st));	
		
		std::chrono::milliseconds t( (int) (d * 1000) );
		if (d == 0) 
		{
			std::cout << "TRUE\n";
			t = std::chrono::hours(1);
		}
		
		auto time_limit = std::chrono::system_clock::now() + t;

		std::pair<
			int, std::chrono::system_clock::time_point> p
			= { cfd, time_limit };
		
		blocklist.insert(p);
		
		if (queues.find(tokens[2]) != queues.end())
		{
			queues[tokens[2]].push(cfd);
		}
		else
		{
			std::queue<int> q;
			
			q.push(cfd);
			
			queues.insert(
				std::pair<std::string, 
				std::queue<int>>
				(tokens[2], q));		
		}	
	}	
	return false; 
}

void Server::BLPOP_RESOLVE(std::string key)
{
	auto it = queues.find(key);
	if (it != queues.end())
	{
		while (lists[key].size() != 0 && !it->second.empty())
		{
			auto p = it->second.front(); 
			it->second.pop(); 
			if (blocklist.find(p) == blocklist.end()) continue; 
			auto r = "*2\r\n$" + key + "$" + lists[key][0];
			sendData(p,1, r);
			blocklist.erase(p);
			lists[key].erase(lists[key].begin(),
										lists[key].begin() + 1);
		}
		if (lists[key].size() == 0) lists.erase(key);	
	}
}

void Server::TYPE()
{
	if (dict.find(tokens[2]) != dict.end())
	{
		response = "$6\r\nstring\r\n";
	} else if (lists.find(tokens[2]) != lists.end())
	{
		response = "$4\r\nlist\r\n";
	} else if (streams.find(tokens[2]) != streams.end())
	{
		response = "$6\r\nstream\r\n";
	} else
	{
		response = "$4\r\nnone\r\n";
	}
}

void Server::resolveID()
{
	if (tokens[3] == "1\r\n*\r\n")
	{
		auto now = std::chrono::system_clock::now();
    auto duration_since_epoch = now.time_since_epoch();
    long long milliseconds_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epoch).count();
		auto st = std::to_string(milliseconds_since_epoch) + "-0";
		tokens[3] = std::to_string(st.size()) + "\r\n" + st + "\r\n";

	}
	else if (tokens[3].find("*") != std::string::npos)
	{
		if (streams.find(tokens[2]) != streams.end())
		{		
			auto old = streams[tokens[2]].rbegin()->first;		
			auto start = old.find("\\",0) + 4;
			auto end = old.size() - 2;
			auto curr_index = old.substr(start, end - start);
			auto temp = curr_index.find("-", 0);
			auto curr_time = curr_index.substr(0,temp);	
			std::cout <<"Current time: " << curr_time << " x";
			
			start = tokens[3].find("\\",0) + 4;
			end = tokens[3].size() - 2;		
			auto n_id = tokens[3].substr(start, end - start);
			temp = n_id.find("-", 0);
			auto n_time = n_id.substr(0,temp); 
			std::cout <<"New time: " << n_time << " y";
			std::string new_index;			

			if (curr_time == n_time)
			{
				auto curr_num = curr_index.substr(temp + 1, curr_index.size() - temp - 1);
				auto new_num = std::to_string(std::stoi(curr_num) + 1); 
				new_index = curr_time + "-" + new_num;			
			} else 
			{
				new_index = n_time + "-0"; 
			}
			 
			tokens[3] = std::to_string(new_index.size())
				+ "\r\n" + new_index + "\r\n";
		} 
		else
		{
			tokens[3] = "3\r\n0-1\r\n";
		}
	}
}

void Server::XADD()
{
	resolveID(); 	
	if (streams.find(tokens[2]) != streams.end())
	{
		if (tokens[3] == "3\r\n0-0\r\n")
		{
			response = "-ERR The ID specified in XADD must be greater than 0-0\r\n";
		}	
		else if (streams[tokens[2]].rbegin()->first >= tokens[3])
		{
			response = "-ERR The ID specified in XADD is equal or smaller than the target stream top item\r\n";
		} 
		else if (streams[tokens[2]].find(tokens[3]) != streams[tokens[2]].end())
		{
			auto it = streams[tokens[2]].find(tokens[3]); 
			int x = 4; 
			while (x + 1 < tokens.size())
			{
				std::pair<std::string, std::string> p = { tokens[x], tokens[x+1] };
				it->second.insert(p); 
				x += 2; 
			}	
			response = "$" + tokens[3];
		}
		else
		{
			std::map<std::string, std::string> temp; 
			int x = 4; 
			while (x + 1 < tokens.size())
			{
				std::pair<std::string, std::string> p = { tokens[x], tokens[x+1] };
				temp.insert(p); 
				x += 2; 
			}	
			streams[tokens[2]][tokens[3]] = temp;			
			response = "$" + tokens[3];
		}	
	}
	else 
	{
		std::map<std::string, std::string> temp; 
		int x = 4; 
		while (x + 1 < tokens.size())
		{
			std::pair<std::string, std::string> p = { tokens[x], tokens[x+1] };
			temp.insert(p); 
			x += 2; 
		}
		
		std::map<std::string, std::map<std::string, std::string>> temp2; 
		temp2[tokens[3]] = temp;
		std::pair<std::string,
			std::map<std::string,
				std::map<std::string, std::string>>> p = { tokens[2], temp2};
		
		streams.insert(p);
		response = "$" + tokens[3];
	}
}

void Server::XRANGE()
{
	auto d = streams[tokens[2]];
	if (tokens[3].find("-",0) == std::string::npos) 
	{
		auto start = tokens[3].find("\\",0) + 4;
		auto end = tokens[3].size() - 2;
		auto time = tokens[3].substr(start, end - start); 
		time = time + "-0"; 
		tokens[3] = std::to_string(time.size()) + "\r\n" + time + "\r\n"; 
	}
	
	auto start = tokens[4].find("\\",0) + 4;
	auto end = tokens[4].size() - 2;
	auto time = tokens[4].substr(start, end - start);
	int i = time.size() - 1; 
	while (time[i] == '9')
	{
		time[i] = '0';
		i--; 
	}
	time[i]++; 
	
	if (tokens[4].find("-",0) == std::string::npos) 
	{
		
		time = time + "-0"; 
	}

	tokens[4] = std::to_string(time.size()) + "\r\n" + time + "\r\n"; 
	auto it0 = d.find(tokens[3]); 
	if (it0 == d.end()) it0 = d.begin(); 
	auto it1 = d.find(tokens[4]); 
	
	std::cout << it0->second.begin()->first;

	int c = 0; 
	std::string x = ""; 
	for (auto it = it0; it != it1; it++)
	{
		c++;
		auto entry = it->second;		
		auto r = "*2\r\n$" + it->first + "*" + std::to_string(entry.size() * 2) + "\r\n"; 
		for (auto it2 = entry.begin(); it2 != entry.end(); it2++)
		{
			r += "$" + it2->first; 
			r += "$" + it2->second;
		}
		x += r; 
	}
	response = "*" + std::to_string(c) + "\r\n" + x; 
}

void Server::XREAD()
{
	int num = (tokens.size() - 3)/2; 
	int j = 0; 
	std::vector<
		std::pair<
			std::map<std::string,std::map<std::string, std::string>>::iterator,	
			std::map<std::string,std::map<std::string, std::string>>::iterator
	>> vp; 

	while (j < num)
	{
		auto d = streams[tokens[3 + j]];
		
		vp.push_back(std::pair<
				std::map<std::string,std::map<std::string, std::string>>::iterator,	
				std::map<std::string,std::map<std::string, std::string>>::iterator
		> (streams[tokens[3 + j]].begin(), streams[tokens[3 + j]].end()));
		
		if (streams[tokens[3 + j]].find(tokens[3 + num + j]) != streams[tokens[3 + j]].end()) 
		{
			vp[j].first = streams[tokens[3 + j]].find(tokens[3 + num + j]);
			vp[j].first++; 
		}	
		std::cout << "ID: " <<  vp[0].first->first << "END\n"; 
		j++; 
	}
	response = "*" + std::to_string(num) + "\r\n";
	int c = 0; 
	
	std::cout << vp[0].first->first << "ED\n";
	
	for (int o = 0; o < vp.size(); o++)
	{
		auto p = vp[o];
		std::string w = "*2\r\n$" + tokens[3 + c]; 
		std::string r = "";
		c++;
		int d = 0; 
		std::cout << "ID2 " << p.first->first << "END2\n"; 
		
		for (auto i = p.first; i != p.second; i++)
		{
			d++; 
			auto entry = i->second; 
			auto x = "*2\r\n$" + i->first + "*" + std::to_string(entry.size() * 2) + "\r\n";
						
			for (auto i1 = entry.begin(); i1 != entry.end(); i1++)
			{
				x += "$" + i1->first; 
				x += "$" + i1->second; 
			}
			r += x; 
		}
			
		w += "*" + std::to_string(d) + "\r\n" + r;
		response += w; 
	}
}

bool Server::commandCenter(int cfd)
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
	} else if (tokens[1] == "5\r\nBLPOP\r\n")
	{
		return BLPOP(cfd);
	} else if (tokens[1] == "4\r\nTYPE\r\n")
	{
		TYPE();
	} else if (tokens[1] == "4\r\nXADD\r\n")
	{
		XADD();
	} else if (tokens[1] == "6\r\nXRANGE\r\n")
	{
		XRANGE();
	} else if (tokens[1] == "5\r\nXREAD\r\n")
	{
		XREAD();
	}

	return true;
}

bool Server::getInput(int& i)
{
	int num_bytes =
		recv(clientfds[i], buffer, sizeof(buffer) - 1, 0);

	if (num_bytes == 0)
	{
		close(clientfds[i]);
		std::cout << "Disconnected " << clientfds[i] << "\n"; 
		clientfds.erase(clientfds.begin() + i); 
		i--;
		return false; 
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
	return true;
}

void Server::sendData(int& i, int type)
{
	int cfd = i; 
	if (type == 0) cfd = clientfds[i]; 

	int m = send(cfd,
		response.c_str(),
		response.size(), 0);
	if (m < 0) 
	{
	std::cerr << "Error in send\n"; 
	std::printf("Socket error code %d\n", errno); 
	}
}

void Server::sendData(int& i, int type, std::string r)
{
	int cfd = i; 
	if (type == 0) cfd = clientfds[i]; 

	int m = send(cfd,
		r.c_str(),
		r.size(), 0);
	if (m < 0) 
	{
	std::cerr << "Error in send\n"; 
	std::printf("Socket error code %d\n", errno); 
	}
}

void Server::controller()
{		
	bzero(buffer, 256);

	int i = 0;

	while (i < clientfds.size())
	{
		if (FD_ISSET(clientfds[i], &masterfds))
		{ 
			if (!getInput(i)) continue; // check client closed
			if (commandCenter(clientfds[i])) sendData(i,0);						
		}
		
		else if (blocklist.find(clientfds[i]) != blocklist.end())
		{
			auto now = std::chrono::system_clock::now(); 
			auto r = "*-1\r\n";
			 
			if (blocklist[clientfds[i]] <= now)
			{
				blocklist.erase(clientfds[i]);
				sendData(i,0,r);
				continue;			
			} 
		}
		i++;
	}
}

void Server::loop()
{
	// reInit(); 
	while (true)
	{		
		if (reInit()) break;
		getClients(); 
		controller();
	}
}

Server::~Server()
{
	std::cout << "Passed through\n";
	clientfds.clear();
	streams.clear(); 
	queues.clear(); 
	blocklist.clear(); 
	lists.clear(); 
	times.clear(); 
	dict.clear(); 
	tokens.clear(); 
	close(server_fd);
}
