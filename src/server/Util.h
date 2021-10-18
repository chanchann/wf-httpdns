#ifndef _UTIL_H_
#define _UTIL_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string>
#include <workflow/WFTaskFactory.h>

class Util
{
public:
    static std::string ip_bin_to_str(void *sa_sin_addr, bool ipv4 = true);

    static std::string get_peer_addr_str(WFHttpTask *server_task);
};

#endif // _UTIL_H_
