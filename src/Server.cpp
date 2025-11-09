#include "../headers/Server.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <netdb.h>
#include <set>
#include <sstream>
#include <string>
#include <strings.h>
#include <unistd.h>
#include <utility>
#include <vector>
#include <filesystem>

Server::Server(int port, std::string r, std::string dir
							 , std::string dbfilename)
{
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) 
	{
   std::cerr << "Failed to create server socket\n";
  }

	role = r;
	s_port = port; 
	config["dir"] = dir;
	config["dbfilename"] = dbfilename;
	auto filename = config["dir"] + "/" + config["dbfilename"]; 
	std::ifstream inputFile(filename, std::ifstream::binary);	
	if (inputFile.good() && !(filename == "/")) readingDB(inputFile);

	// std::cout << (std::string) arr;
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
  server_addr.sin_port = htons(port);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr,
					 sizeof(server_addr)) != 0) 
	{
    std::cerr << "Failed to bind to port \n";
  }
		
	int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) 
	{
    std::cerr << "listen failed\n";  
  }
}

void Server::readingDB(std::ifstream& inputFile)
{
	char c;
	bool flag = false; 
	std::vector<char> chars;
	while (inputFile.get(c))
	{
		if (flag) chars.push_back(c);	
		auto cha = static_cast<unsigned char>(c);

		if (cha == 0xFB)
		{
			flag = true;		
		}
		if (cha == 0xFF)
		{
			flag = false;
			chars.erase(chars.end());
			break;
		}
	}

	inputFile.close();
	int normal_size = static_cast<int>(chars[0]);
	int expiry_size = static_cast<int>(chars[1]);
	int i = 2;
	int first_length;
	std::string key;	
	int second_length;
	std::string value;	
	int count = 0; 

	while (count < normal_size)
	{
		auto cha = static_cast<unsigned char>(chars[i]); 
		uint64_t time = 0;

		if (cha == 0xFC) 
		{
			i++;
			uint64_t t;
			char buf[sizeof(t)]; 
			std::copy(chars.begin() + i, chars.begin() + i + sizeof(t), (char *) &t);
			time = t; 
			i += sizeof(t);  
		}
		else if (cha == 0xFD) 
		{
			i++;
			uint32_t t;
			char buf[sizeof(t)]; 
			std::copy(chars.begin() + i, chars.begin() + i + sizeof(t), (char *) &t);
			time = t * 1000; 
			i += sizeof(t);  
		}
		
		i++; 
		first_length = static_cast<int>(chars[i]);
		i++; 
		key = "";
		for (int j = i; j < i + first_length; j++) key += chars[j];	
				
		second_length = static_cast<int>(chars[i + first_length]);
		i++; 
		value = ""; 
		for (int j = i + first_length;
					j < i + first_length + second_length;
					j++) value += chars[j];
		key = std::to_string(key.size()) + "\r\n" + key + "\r\n"; 
		value = std::to_string(value.size()) + "\r\n" + value + "\r\n"; 
		dict[key] = value;
		if (time)
		{
			std::chrono::milliseconds duration(time);	
			std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> time_pt(duration);
			times[key] = time_pt;
		} 
		i += first_length + second_length;
		count++; 
	}
}

void Server::replicatingMaster(std::string loc)
{
	auto hostname = loc.substr(0, loc.find(" ",0)); 
	auto portno = std::stoi(loc.substr(loc.find(" ",0)));

	struct sockaddr_in serv_addr;
	struct hostent *server;

	int master_fd = socket(AF_INET, SOCK_STREAM, 0);

	char buffer[256];

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY; 
	serv_addr.sin_port = htons(portno);

	if (connect(master_fd,(struct sockaddr *)
						 &serv_addr,sizeof(serv_addr)) < 0) std::cerr << "ERROR connecting";

	response = "*1\r\n$4\r\nPING\r\n"; 

	if (send(master_fd,response.c_str(), response.size(),0) < 0)
	{
		std::cerr << "Error in send\n"; 
		std::printf("Socket error code %d\n", errno); 
	}
	
	bzero(buffer, sizeof(buffer));
	int num_bytes =
		recv(master_fd, buffer, sizeof(buffer) - 1, 0);


	response =
		"*3\r\n$8\r\nREPLCONF\r\n$14\r\nlistening-port\r\n$4\r\n" + std::to_string(s_port) + "\r\n"; 

	if (send(master_fd,response.c_str(), response.size(),0) < 0)
	{
		std::cerr << "Error in send\n"; 
		std::printf("Socket error code %d\n", errno); 
	}

	bzero(buffer, sizeof(buffer));
	num_bytes =
		recv(master_fd, buffer, sizeof(buffer) - 1, 0);
	response = "*3\r\n$8\r\nREPLCONF\r\n$4\r\ncapa\r\n$6\r\npsync2\r\n"; 

	if (send(master_fd,response.c_str(), response.size(),0) < 0)
	{
		std::cerr << "Error in send\n"; 
		std::printf("Socket error code %d\n", errno); 
	}
	
	bzero(buffer, sizeof(buffer));
	num_bytes =
		recv(master_fd, buffer, sizeof(buffer) - 1, 0);
	response = "*3\r\n$5\r\nPSYNC\r\n$1\r\n?\r\n$2\r\n-1\r\n";
	if (send(master_fd,response.c_str(), response.size(),0) < 0)
	{
		std::cerr << "Error in send\n"; 
		std::printf("Socket error code %d\n", errno); 
	}

	response = "";
	clientfds.push_back(master_fd);
}

void Server::applyingReplicas()
{
	for (auto it = replicas.begin(); it != replicas.end(); it++)
	{
		int i = *it; 
		sendData(i, input);
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
		std::cout << "Got here2\n"; 
		for (auto i : tokens) std::cout << "ID " << i << " ED\n"; 
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
	if (dict.find(tokens[2]) == dict.end())
	{
		response = "$-1\r\n";	
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
				dict.erase(tokens[2]);
				times.erase(it); 
				response = "$-1\r\n";
			}
		}	
	}
}

void Server::INCR()
{
	if (dict.find(tokens[2]) == dict.end())
	{
		tokens.push_back("1\r\n1\r\n"); 
		setValue();
		response = ":1\r\n"; 
	} 
	else 
	{
		auto d = dict[tokens[2]]; 
		auto start = d.find("\r\n",0) + 2; 
		auto end = d.find("\r\n", start); 
		auto x = d.substr(start, end - start); 
		try {
			auto y = std::to_string(std::stoi(x) + 1); 
			dict[tokens[2]] = std::to_string(y.size()) + "\r\n" + y + "\r\n"; 
			response = ":" + y + "\r\n";
		} catch (const std::invalid_argument& e) {
			response = "-ERR value is not an integer or out of range\r\n";  
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
			sendData(p, r);
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
		response = "+string\r\n";
	} else if (lists.find(tokens[2]) != lists.end())
	{
		response = "+list\r\n";
	} else if (streams.find(tokens[2]) != streams.end())
	{
		response = "+stream\r\n";
	} else
	{
		response = "+none\r\n";
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
	XREAD_BLOCK_RESOLVE(tokens[2]); 
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

void Server::XREAD_BLOCK(int cfd)
{
	int st = tokens[3].find("\r\n",0) + 2; 
	int ed = tokens[3].find("\r\n", st); 
	int d = std::stoi(tokens[3].substr(st, ed - st));	
	
	std::chrono::milliseconds t( d );
	if (d == 0) 
	{
		t = std::chrono::hours(1);
	}
	
	auto time_limit = std::chrono::system_clock::now() + t; 
	
	std::pair<
		int, std::chrono::system_clock::time_point> p
		= { cfd, time_limit };
	
	blocklist.insert(p);
	
	if (stream_queues.find(tokens[5]) != stream_queues.end())
	{
		stream_queues[tokens[5]].push_back(
			std::pair<
				std::string, int>(tokens[6], cfd));
		
		std::sort(stream_queues[tokens[5]].begin()
						 ,stream_queues[tokens[5]].end());

		std::reverse(stream_queues[tokens[5]].begin()
						 ,stream_queues[tokens[5]].end());	
	}
	else
	{
		std::vector<
			std::pair<
				std::string, int>> q;
		
		q.push_back(std::pair<std::string, int>(tokens[6], cfd));
		
		stream_queues.insert(
			std::pair<std::string, 
				std::vector<
					std::pair<
						std::string, int>>>
			(tokens[5], q));		
	}

}	

void Server::XREAD_BLOCK_RESOLVE(std::string key)
{
	auto it = stream_queues.find(key);
	if (it != stream_queues.end())
	{
		auto it2 = streams[key].rbegin();
		int i = it->second.size() - 1; 
		while (i >= 0 && it->second[i].first < it2->first)
		{
			auto p = it->second[i].second; 
			if (blocklist.find(p) == blocklist.end()) 
			{
				it->second.erase(it->second.begin() + i); 
				i--;			
				continue;
			}
			auto entry = it2->second;
			auto r = "*1\r\n*2\r\n$" + key + "*1\r\n*2\r\n$" + it2->first
							+ "*" + std::to_string(entry.size() * 2) + "\r\n"; 
			
			for (auto x = entry.begin(); x != entry.end(); x++)
			{
				r += "$" + x->first; 
				r += "$" + x->second; 
			}
			auto time = std::chrono::system_clock::now(); 
			sendData(p,r);
			blocklist.erase(p);
			it->second.erase(it->second.begin() + i); 
			i--; 
		}
		if (it->second.size() == 0) stream_queues.erase(key);	
	}
}

void Server::MULTI(int cfd)
{
	auto temp(tokens); 
	std::string r = "+QUEUED\r\n";
	
	if (tokens[1] == "7\r\ndiscard\r\n")
	{
		mul.erase(cfd);		
		queued_commands.erase(cfd);		
		response = "+OK\r\n";
		sendData(cfd,response);
	} 
	else if (tokens[1] == "4\r\nexec\r\n")
	{
		mul.erase(cfd);
		if (queued_commands.find(cfd) == queued_commands.end())
		{
			response = "*0\r\n";
			sendData(cfd,response);		
		}	
		else
		{
			r = "*" + std::to_string(queued_commands[cfd].size()) + "\r\n"; 
			while (!queued_commands[cfd].empty())
			{
				tokens = queued_commands[cfd].front(); 
				queued_commands[cfd].pop(); 
				if (commandCenter(cfd)) r += response;
			}
			sendData(cfd, r); 
			queued_commands.erase(cfd); 
		}
	} 
	else if (queued_commands.find(cfd) != queued_commands.end())
	{
		queued_commands[cfd].push(temp);
		sendData(cfd,r);	
	}
	else
	{
		std::queue<std::vector<std::string>> q; 
		q.push(temp); 
		queued_commands[cfd] = q; 
		sendData(cfd,r);	
	}

}

void Server::WAIT()
{	
	fd_set tempfds; 
	std::vector<int> temps; 
	temps.assign(replicas.begin(), replicas.end()); 
	auto maxfd = server_fd; 
	
	auto start = tokens[2].find("\r\n",0) + 2; 
	auto end = tokens[2].find("\r\n",start); 
	auto num = std::stoi(tokens[2].substr(start, end - start));

	start = tokens[3].find("\r\n",0) + 2; 
	end = tokens[3].find("\r\n",start); 
	auto wait = std::stoi(tokens[3].substr(start, end - start));
	auto wait_time = std::chrono::milliseconds(wait); 

	auto now_time = std::chrono::system_clock::now(); 
	auto limit = now_time + wait_time; 

	auto current = 0; 
	int activity;
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	auto spark = "*3\r\n$8\r\nREPLCONF\r\n$6\r\nGETACK\r\n$1\r\n*\r\n"; 
	
	for (auto rfd : temps) 
	{
		sendData(rfd, spark);
	}	

	while (current < num && now_time < limit)
	{
		FD_ZERO(&tempfds); 
		FD_SET(server_fd, &tempfds);
		for (auto rfd : temps) 
		{
			FD_SET(rfd, &tempfds);
			if (rfd > maxfd) maxfd = rfd;
		}
		activity = select(maxfd + 1, &tempfds, NULL, NULL, &tv); 

		if (activity < 0) 
		{
			std::cerr << "select error\n"; 
			std::cout << "Error code: " << errno <<  "\n"; 
			break; 
		} 

		int i = 0; 

		while (i < temps.size())
		{
			if (FD_ISSET(temps[i], &tempfds))
			{
				int num_bytes = recv(temps[i], buffer, sizeof(buffer) - 1, 0);
				current += 1;
				temps.erase(temps.begin() + i); 
				i--; 
			}
			
			i++;
		}
		now_time = std::chrono::system_clock::now(); 
	}
	response = ":" + std::to_string(current) + "\r\n";

}

auto cmp = [](std::pair<std::string, std::string> p1, std::pair<std::string, std::string> p2) 
{
	auto h1 = p1.first;
	auto h2 = p2.first; 
	try {
		auto v1 = std::stod(h1);
		auto v2 = std::stod(h2); 
		if (v1 == v2) return p1.second < p2.second;
		return v1 < v2; 
	} catch (const std::invalid_argument& e) { 
		if (h1 == h2) return p1.second < p2.second;
		return h1 < h2;// Not a valid number format
  }
};


void Server::ZADD()
{
	auto key = tokens[2]; 
	auto start = tokens[3].find("\r\n",0) + 2; 
	auto end = tokens[3].find("\r\n",start); 
	auto val = tokens[3].substr(start,end - start);
	std::string r = "1"; 

	start = tokens[4].find("\r\n",0) + 2; 
	end = tokens[4].find("\r\n",start); 
	auto label = tokens[4].substr(start,end - start); 

	if (sorted_sets[key].find(label) != sorted_sets[key].end())
	{
		auto old_val = sorted_sets[key][label];
		std::pair<std::string, std::string> p = { old_val, label };
		int c = 0; 
		for (auto it = set_ordering[key].begin(); *it != p && it != set_ordering[key].end(); it++)  c++;
		set_ordering[key].erase(set_ordering[key].begin() + c);
		r = "0"; 
	}
	
	sorted_sets[key][label] = val; 
	std::pair<std::string, std::string> q = { val, label };
	set_ordering[key].push_back(q);
	std::sort(set_ordering[key].begin(), set_ordering[key].end(), cmp); 	
	response = ":" + r + "\r\n"; 

}
 

void Server::ZRANK()
{
	if (set_ordering.find(tokens[2]) == set_ordering.end())
	{
		response = "$-1\r\n";	
		return;
	}

	int c = 0; 
	auto start = tokens[3].find("\r\n",0) + 2; 
	auto send = tokens[3].find("\r\n",start); 
	auto label = tokens[3].substr(start,send - start);

	for (auto i = set_ordering[tokens[2]].begin(); i->second != label & i != set_ordering[tokens[2]].end(); i++) c++;
	response = ":" + std::to_string(c) + "\r\n"; 
	if (c == set_ordering[tokens[2]].size()) response = "$-1\r\n";
}

void Server::ZRANGE()
{
	auto key = tokens[2]; 
	response = "*0\r\n";

	if (set_ordering.find(key) == set_ordering.end())
	{
		return;
	}

	auto ordering = set_ordering[key]; 
	auto start = tokens[3].find("\r\n",0) + 2; 
	auto end = tokens[3].find("\r\n",start); 
	auto lower_bound = std::stoi(tokens[3].substr(start, end - start));
	if (lower_bound < 0) lower_bound = ordering.size() + lower_bound; 
	if (lower_bound < 0) lower_bound = 0; 	

	start = tokens[4].find("\r\n",0) + 2; 
	end = tokens[4].find("\r\n",start); 
	auto upper_bound = std::stoi(tokens[4].substr(start, end - start));
	if (upper_bound < 0) upper_bound = ordering.size() + upper_bound; 
	if (upper_bound < 0) upper_bound = 0;

	if (lower_bound > ordering.size() || lower_bound > upper_bound)
	{
		return; 
	} 
	else if (upper_bound > ordering.size()) 
	{
		upper_bound = ordering.size() - 1;	
	} 

	auto i = lower_bound;
	response = "*" + std::to_string(upper_bound - lower_bound + 1) + "\r\n"; 
	while (i <= upper_bound)
	{
		auto it = std::next(ordering.begin(), i); 
		std::string label = it->second;
		std::cout << "ID: " << label << " ED\n"; 
		response += "$" + std::to_string(label.size()) + "\r\n" + label + "\r\n"; 
		i++;
	}

}

void Server::ZCARD()
{
	auto key = tokens[2]; 
	if (set_ordering.find(key) == set_ordering.end()) 
	{
		response = "+0\r\n"; 
	}

	auto o = set_ordering[key]; 
	response = ":" + std::to_string(o.size()) + "\r\n"; 
}

void Server::ZSCORE()
{
	auto key = tokens[2]; 
	response = "$-1\r\n";

	if (sorted_sets.find(key) == sorted_sets.end()) return; 

	auto sset = sorted_sets[key]; 
	auto start = tokens[3].find("\r\n",0) + 2; 
	auto end = tokens[3].find("\r\n",start); 
	auto label = tokens[3].substr(start, end - start); 
	
	if (sset.find(label) == sset.end()) return; 
	
	std::ostringstream oss;
	auto val = sset[label];	
	oss << std::setprecision(17) << val;   
	std::string str = oss.str(); 
	response = "$" + std::to_string(str.size()) + "\r\n" + str + "\r\n"; 
}

void Server::ZREM()
{
	auto key = tokens[2]; 
	auto& sset = sorted_sets[key]; 

	auto start = tokens[3].find("\r\n",0) + 2;
	auto end = tokens[3].find("\r\n",start);
	auto label = tokens[3].substr(start, end - start); 
	response = ":0\r\n";

	if (sset.find(label) == sset.end())
	{
		return;
	}

	auto val = sset[label]; 
	sset.erase(label); 
	std::pair< std::string, std::string> p = { val, label };
	auto& s_order = set_ordering[key]; 
	int c = 0; 
	for (auto it = s_order.begin(); *it != p && it != s_order.end(); it++)  c++;
	s_order.erase(s_order.begin() + c); 
	
	response = ":1\r\n"; 

}

std::string encodeGeohash(double latitude, double longitude, int precision = 12) 
{
	const std::string BASE32_CHARS = "0123456789bcdefghjkmnpqrstuvwxyz";

	double lat_min = -85.05112878, lat_max = +85.05112878;
	double lon_min = -180.0, lon_max = 180.0;

	std::string geohash_bits;
	bool is_even_bit = true; // True for longitude, false for latitude

	for (int i = 0; i < precision * 5; ++i) 
	{ // 5 bits per character in Base32
		if (is_even_bit) 
		{ // Longitude
			double mid = (lon_min + lon_max) / 2.0;
			if (longitude > mid) 
			{
				geohash_bits += '1';
				lon_min = mid;
			} 
			else 
			{
				geohash_bits += '0';
				lon_max = mid;
			}
		} 
		else 
		{ // Latitude
			double mid = (lat_min + lat_max) / 2.0;
			if (latitude > mid) 
			{
				geohash_bits += '1';
				lat_min = mid;
			} 
			else 
			{
				geohash_bits += '0';
				lat_max = mid;
			}
		}
		is_even_bit = !is_even_bit;
	}

	std::string geohash_string;
	for (size_t i = 0; i < geohash_bits.length(); i += 5) 
	{
		std::string five_bits = geohash_bits.substr(i, 5);
		int val = std::stoi(five_bits, nullptr, 2); // Convert binary string to integer
		geohash_string += BASE32_CHARS[val];
	}

	return geohash_string;
}

uint64_t spread_int32_to_int64(uint32_t v) 
{
    uint64_t result = v;
    result = (result | (result << 16)) & 0x0000FFFF0000FFFFULL;
    result = (result | (result << 8)) & 0x00FF00FF00FF00FFULL;
    result = (result | (result << 4)) & 0x0F0F0F0F0F0F0F0FULL;
    result = (result | (result << 2)) & 0x3333333333333333ULL;
    result = (result | (result << 1)) & 0x5555555555555555ULL;
    return result;
}

uint64_t interleave(uint32_t x, uint32_t y) 
{
    uint64_t x_spread = spread_int32_to_int64(x);
    uint64_t y_spread = spread_int32_to_int64(y);
    uint64_t y_shifted = y_spread << 1;
    return x_spread | y_shifted;
}

uint64_t encode(double latitude, double longitude) 
{
    // Normalize to the range 0-2^26
    double normalized_latitude = pow(2, 26) * (latitude - MIN_LATITUDE) / LATITUDE_RANGE;
    double normalized_longitude = pow(2, 26) * (longitude - MIN_LONGITUDE) / LONGITUDE_RANGE;

    // Truncate to integers
    uint32_t lat_int = (uint32_t)normalized_latitude;
    uint32_t lon_int = (uint32_t)normalized_longitude;

    return interleave(lat_int, lon_int);
}

void Server::GEOADD()
{
	auto key = tokens[2];
	auto start = tokens[3].find("\r\n",0) + 2; 
	auto end = tokens[3].find("\r\n",start); 
	auto lon = std::stod(tokens[3].substr(start, end - start)); 
	
	start = tokens[4].find("\r\n",0) + 2; 
	end = tokens[4].find("\r\n",start); 
	auto lat = std::stod(tokens[4].substr(start, end - start)); 

	if (lon < -180 || lon > 180 || lat < -85.05112878 || lat > +85.05112878)
	{
		std::ostringstream oss; 
		oss << std::setprecision(20) << lon << "," << std::setprecision(20)
			<< lat; 
		response = "-ERR invalid longitude,latitude pair " + oss.str() + "\r\n";
		return;
	}

	start = tokens[5].find("\r\n",0) + 2; 
	end = tokens[5].find("\r\n",start); 
	auto label = tokens[5].substr(start, end - start); 

	auto hash =  std::to_string(encode(lat, lon));

	sorted_sets[key][label] = hash;
	std::pair< std::string, std::string> p = { hash, label };
	set_ordering[key].push_back(p); 
	std::sort(set_ordering[key].begin(), set_ordering[key].end(), cmp); 	
	response = ":1\r\n"; 
}

typedef struct 
{
    double latitude;
    double longitude;
} coordinates_t;

uint32_t compact_int64_to_int32(uint64_t v) 
{
    v = v & 0x5555555555555555ULL;
    v = (v | (v >> 1)) & 0x3333333333333333ULL;
    v = (v | (v >> 2)) & 0x0F0F0F0F0F0F0F0FULL;
    v = (v | (v >> 4)) & 0x00FF00FF00FF00FFULL;
    v = (v | (v >> 8)) & 0x0000FFFF0000FFFFULL;
    v = (v | (v >> 16)) & 0x00000000FFFFFFFFULL;
    return (uint32_t)v;
}

coordinates_t convert_grid_numbers_to_coordinates(uint32_t grid_latitude_number, uint32_t grid_longitude_number) 
{
    coordinates_t result;
    
    // Calculate the grid boundaries
    double grid_latitude_min = MIN_LATITUDE + LATITUDE_RANGE * (grid_latitude_number / pow(2, 26));
    double grid_latitude_max = MIN_LATITUDE + LATITUDE_RANGE * ((grid_latitude_number + 1) / pow(2, 26));
    double grid_longitude_min = MIN_LONGITUDE + LONGITUDE_RANGE * (grid_longitude_number / pow(2, 26));
    double grid_longitude_max = MIN_LONGITUDE + LONGITUDE_RANGE * ((grid_longitude_number + 1) / pow(2, 26));
    
    // Calculate the center point of the grid cell
    result.latitude = (grid_latitude_min + grid_latitude_max) / 2;
    result.longitude = (grid_longitude_min + grid_longitude_max) / 2;
    
    return result;
}

coordinates_t decode(uint64_t geo_code) 
{
    // Align bits of both latitude and longitude to take even-numbered position
    uint64_t y = geo_code >> 1;
    uint64_t x = geo_code;
    
    // Compact bits back to 32-bit ints
    uint32_t grid_latitude_number = compact_int64_to_int32(x);
    uint32_t grid_longitude_number = compact_int64_to_int32(y);
    
    return convert_grid_numbers_to_coordinates(grid_latitude_number, grid_longitude_number);
}

void Server::GEOPOS()
{
	auto key = tokens[2]; 
	std::string nil = "*-1\r\n";
	auto ssize = tokens.size() - 3;
	response = "*" + std::to_string(ssize) + "\r\n";

	if (set_ordering.find(tokens[2]) == set_ordering.end())
	{
		for (int i = 0; i < ssize; i++) response += nil;
		return; 
	}

	auto& sset = sorted_sets[key]; 
	std::stringstream lat, lon;
	std::string x, y; 

	for (int i = 1; i <= ssize; i++) 
	{
		auto start = tokens[2 + i].find("\r\n",0) + 2; 
		auto end = tokens[2 + i].find("\r\n",start); 
		auto label = tokens[2 + i].substr(start, end - start);
		
		if(sset.find(label) != sset.end())
		{
			auto val = std::stod(sset[label]); 
			auto c = decode(val);
			std::string loc = "*2\r\n"; 

			lon.str(""); 
			lon << std::setprecision(17) << c.longitude;
			y = lon.str();	
			std::cout << "y: " << y << "\n"; 
			loc += "$" + std::to_string(y.size()) + "\r\n" + y + "\r\n";
			
		
			lat.str("");
			lat << std::setprecision(17) << c.latitude;
			x = lat.str();
			std::cout << "x: " << x << "\n"; 
			loc += "$" + std::to_string(x.size()) + "\r\n" + x + "\r\n"; 
			response += loc;		
		}
		else 
		{
			response += nil;
		}
	}
}

double DegreeToRadian(double angle)
{
	return M_PI * angle / 180.0;
}

double HaversineDistance(const double lon1, const double lat1, 
												 const double lon2, const double lat2)
{
	double lonRad1 = DegreeToRadian(lon1);
	double lonRad2 = DegreeToRadian(lon2);
	double latRad1 = DegreeToRadian(lat1);
	double latRad2 = DegreeToRadian(lat2);

	double diffLa = latRad2 - latRad1;
	double doffLo = lonRad2 - lonRad1;

	double computation = asin(sqrt(sin(diffLa / 2) * sin(diffLa / 2) + cos(latRad1)
																* cos(latRad2) * sin(doffLo / 2) * sin(doffLo / 2)));
	return 2 * EARTH_RADIUS_IN_METERS * computation;
}

void Server::GEODIST()
{
	auto key = tokens[2]; 
	auto& sset = sorted_sets[key]; 

	auto start = tokens[3].find("\r\n",0) + 2;
	auto end = tokens[3].find("\r\n",start);
	auto place1 = tokens[3].substr(start, end - start);
	std::cout << "val1: " << sset[place1] << "\n"; 
	auto val1 = std::stod(sset[place1]); 
	auto c1 = decode(val1);
	auto lon1 = c1.longitude;
	auto lat1 = c1.latitude; 

	start = tokens[4].find("\r\n",0) + 2;
	end = tokens[4].find("\r\n",start);
	auto place2 = tokens[3].substr(start, end - start);
	std::cout << "val2 " << sset[place2] << "\n";
	auto val2 = std::stod(sset[place2]); 
	auto c2 = decode(val2); 
	auto lon2 = c2.longitude;
	auto lat2 = c2.latitude; 

	auto distance = HaversineDistance(lon1, lat1, lon2, lat2); 
	std::stringstream ss;
	ss << std::setprecision(17) << distance; 
	std::string x = ss.str(); 
	response += "$" + std::to_string(x.size()) + "\r\n" + x + "\r\n";
}

bool Server::commandCenter(int cfd)
{
	if (subscribed_channels.find(cfd) != subscribed_channels.end()
				&& allowed_commands.find(tokens[1]) == allowed_commands.end())
	{
		auto start = tokens[1].find("\r\n",0) + 2;
		auto end = tokens[1].find("\r\n",start); 
		auto command = tokens[1].substr(start, end - start); 
		response = "-ERR Can't execute '" + command + "': only (P|S)SUBSCRIBE / (P|S)UNSUBSCRIBE / PING / QUIT / RESET are allowed in this context\r\n";
		return true;
	}

	if (tokens[1] == "4\r\nping\r\n")
	{
		response = "+PONG\r\n";
		if (subscribed_channels.find(cfd) != subscribed_channels.end()) response = "*2\r\n$4\r\npong\r\n$0\r\n\r\n";
		byte_counter += input.size() * trackingFlag;
		return !trackingFlag; 
	} 
	else if (tokens[1] == "4\r\necho\r\n") 
	{
		response = "$" + tokens[2];
	} 
	else if (tokens[1] == "3\r\nset\r\n")
	{
		setValue(); 	
		offsetUnChanged = false;
		applyingReplicas();
		byte_counter += input.size() * trackingFlag;
		return !trackingFlag; 
	} 
	else if (tokens[1] == "3\r\nget\r\n")
	{
		getValue(); 	
	} 
	else if (tokens[1] == "5\r\nrpush\r\n")
	{
		listPush();
	}
	else if (tokens[1] == "6\r\nlrange\r\n")
	{
		lrange(); 
	} 
	else if (tokens[1] == "5\r\nlpush\r\n")
	{
		listPushLeft();
	} 
	else if (tokens[1] == "4\r\nllen\r\n")
	{
		getLength();
	} 
	else if (tokens[1] == "4\r\nlpop\r\n")
	{
		LPOP();
	} 
	else if (tokens[1] == "5\r\nblpop\r\n")
	{
		return BLPOP(cfd);
	} 
	else if (tokens[1] == "4\r\ntype\r\n")
	{
		TYPE();
	} 
	else if (tokens[1] == "4\r\nxadd\r\n")
	{
		XADD();
	} 
	else if (tokens[1] == "6\r\nxrange\r\n")
	{
		XRANGE();
	} 
	else if (tokens[1] == "5\r\nxread\r\n")
	{
		if (tokens[2] == "7\r\nstreams\r\n")
		{
			XREAD();
		} 
		else if (tokens[2] == "5\r\nblock\r\n")
		{
			XREAD_BLOCK(cfd); 
			return false; 
		}
	} 
	else if (tokens[1] == "4\r\nincr\r\n")
	{
		INCR();
	} 
	else if (tokens[1] == "5\r\nmulti\r\n")
	{
		mul.insert(cfd); 
		response = "+OK\r\n";
	} 
	else if (tokens[1] == "4\r\nexec\r\n")
	{
		response = "-ERR EXEC without MULTI\r\n";
	} 
	else if (tokens[1] == "7\r\ndiscard\r\n")
	{
		response = "-ERR DISCARD without MULTI\r\n";
	} 
	else if (tokens[1] == "4\r\ninfo\r\n")
	{
		auto t = "role:" + role + "\r\nmaster_replid:" + master_replid
			+ "\r\nmaster_repl_offset:" + std::to_string(master_repl_offset);
		response = "$" + std::to_string(t.size()) + "\r\n" + t + "\r\n";
	} 
	else if (tokens[1] == "8\r\nreplconf\r\n")
	{ 
		std::cout << "o1\n"; 
		if (role == "master")
		{
			std::cout << "o2\n"; 
			response = "+OK\r\n";
			replicas.insert(cfd);
		} 

		else if (tokens[2] == "6\r\ngetack\r\n")
		{
			std::cout << "o3\n"; 
			std::cout << "Got here\n";
			trackingFlag = 1; 
			auto b = std::to_string(byte_counter); 
			response = "*3\r\n$8\r\nREPLCONF\r\n$3\r\nACK\r\n$"
				+ std::to_string(b.size()) +"\r\n"+ b +"\r\n";
			byte_counter += input.size() * trackingFlag;
		}
	} 
	else if (tokens[1] == "5\r\npsync\r\n")
	{
		const std::string EMPTY_RDB = 
			"\x52\x45\x44\x49\x53\x30\x30\x31\x31\xfa\x09\x72\x65\x64\x69\x73\x2d\x76\x65\x72\x05\x37\x2e\x32\x2e\x30\xfa\x0a\x72\x65\x64\x69\x73\x2d\x62\x69\x74\x73\xc0\x40\xfa\x05\x63\x74\x69\x6d\x65\xc2\x6d\x08\xbc\x65\xfa\x08\x75\x73\x65\x64\x2d\x6d\x65\x6d\xc2\xb0\xc4\x10\x00\xfa\x08\x61\x6f\x66\x2d\x62\x61\x73\x65\xc0\x00\xff\xf0\x6e\x3b\xfe\xc0\xff\x5a\xa2"; 

		response = "+FULLRESYNC " + master_replid + " " + std::to_string(master_repl_offset) + "\r\n";
		response += "$" + std::to_string(EMPTY_RDB.size()) + "\r\n" + EMPTY_RDB;
	}
	else if (tokens[1] == "4\r\nwait\r\n")
	{
		if (offsetUnChanged) response = ":"
													+ std::to_string(replicas.size()) + "\r\n";
		else
		{
			WAIT(); 
		}
	}
	else if (tokens[1] == "6\r\nconfig\r\n")
	{
		auto start = tokens[3].find("\r\n", 0) + 2; 
		auto end = tokens[3].find("\r\n", start);
		auto field = tokens[3].substr(start, end - start); 
		auto value = config[field]; 

		response = "*2\r\n$" + std::to_string(end - start)
						+ "\r\n" + field + "\r\n$"
						+ std::to_string(value.size()) + "\r\n" + value + "\r\n"; 
	}
	else if (tokens[1] == "4\r\nkeys\r\n")
	{ 
		response = "*" + std::to_string(dict.size()) + "\r\n"; 
		for (auto p : dict) response += "$" + p.first;
	}
	else if (tokens[1] == "9\r\nsubscribe\r\n")
	{
		auto& sc = subscribed_channels[cfd];
		if (sc.find(tokens[2]) != sc.end())
		{
			response = "*3\r\n$" + tokens[1] + "$" + tokens[2] + ":" + std::to_string(sc.size()) + "\r\n";
		}
		else 
		{
			sc.insert(tokens[2]);
			channel_subscribers[tokens[2]].insert(cfd);
			response = "*3\r\n$" + tokens[1] + "$" + tokens[2] + ":" + std::to_string(sc.size()) + "\r\n";
		}
	}
	else if (tokens[1] == "7\r\npublish\r\n")
	{
		auto r = "*3\r\n$7\r\nmessage\r\n$" + tokens[2] + "$" + tokens[3]; 
		for (auto it = channel_subscribers[tokens[2]].begin();
					it != channel_subscribers[tokens[2]].end(); 
						it++)
		{
			int i = *it; 
			sendData(i, r);
		}
		
		response = ":" + std::to_string(channel_subscribers[tokens[2]].size()) + "\r\n"; 
	}
	else if (tokens[1] == "11\r\nunsubscribe\r\n")
	{
		subscribed_channels[cfd].erase(tokens[2]); 
		channel_subscribers[tokens[2]].erase(cfd); 
		response = "*3\r\n$" + tokens[1] + "$" + tokens[2] + ":" + 
			std::to_string(subscribed_channels[cfd].size()) + "\r\n"; 
	}
	else if (tokens[1] == "4\r\nzadd\r\n")
	{
		ZADD(); 
	}
	else if (tokens[1] == "5\r\nzrank\r\n")
	{
		ZRANK();
	}
	else if(tokens[1] == "6\r\nzrange\r\n")
	{
		ZRANGE(); 
	}
	else if(tokens[1] == "5\r\nzcard\r\n")
	{
		ZCARD();
	}
	else if(tokens[1] == "6\r\nzscore\r\n")
	{
		ZSCORE();
	}
	else if(tokens[1] == "4\r\nzrem\r\n")
	{
		ZREM(); 
	}
	else if(tokens[1] == "6\r\ngeoadd\r\n")
	{
		GEOADD(); 
	}
	else if(tokens[1] == "6\r\ngeopos\r\n")
	{ 
		GEOPOS();
	}
	else if(tokens[1] == "7\r\ngeodist\r\n")
	{
		GEODIST();
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
	std::transform(input.begin(), input.end(), input.begin(),
                   [](unsigned char c){ return std::tolower(c); });
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

void Server::sendData(int& i, std::string r)
{
	int cfd = i; 
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
			
			if (mul.find(clientfds[i]) != mul.end())
			{
				MULTI(clientfds[i]);
			} else if (commandCenter(clientfds[i])) sendData(clientfds[i],response);						
		}
		
		else if (blocklist.find(clientfds[i]) != blocklist.end())
		{
			auto now = std::chrono::system_clock::now(); 
			response = "*-1\r\n";
			 
			if (blocklist[clientfds[i]] <= now)
			{
				blocklist.erase(clientfds[i]);
				sendData(clientfds[i],response);
				continue;			
			} 
		}
		i++;
	}
}

void Server::loop()
{
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
