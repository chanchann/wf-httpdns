#include "HttpUtil.h"
#include <spdlog/spdlog.h>

std::string HttpUtil::get_peer_addr_str(WFHttpTask *server_task) {
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