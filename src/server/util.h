#ifndef UTIL_H
#define UTIL_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>   
#include <arpa/inet.h>
#include <string>
#include <workflow/WFTaskFactory.h>

static const int INET_ADDR_LEN = 128;  

// 暂时只支持ipv4
std::string ipBinToStr(void* sa_sin_addr) {
   struct sockaddr_in sa;
   char str[INET_ADDR_LEN];
   inet_ntop(AF_INET, sa_sin_addr, str, INET_ADDR_LEN);
   return str;
}

std::string getPeerAddrStr(WFHttpTask *server_task) {
	char addrstr[128];
	struct sockaddr_storage addr;
	socklen_t addr_len = sizeof addr;
	unsigned short port = 0;
   
	server_task->get_peer_addr((struct sockaddr *)&addr, &addr_len);
	if (addr.ss_family == AF_INET)
	{
		struct sockaddr_in *sin = (struct sockaddr_in *)&addr;
		inet_ntop(AF_INET, &sin->sin_addr, addrstr, 128);
		port = ntohs(sin->sin_port);
	}
	else if (addr.ss_family == AF_INET6)
	{
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&addr;
		inet_ntop(AF_INET6, &sin6->sin6_addr, addrstr, 128);
		port = ntohs(sin6->sin6_port);
	}
	else
		strcpy(addrstr, "Unknown");
	spdlog::trace("Get peer addr : {}", addrstr);
   	return addrstr;
}
#endif // UTIL_H