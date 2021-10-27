#ifndef _HTTPUTIL_H_
#define _HTTPUTIL_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string>
#include <workflow/WFTaskFactory.h>
#include <workflow/StringUtil.h>

class HttpUtil
{
public:
    static std::string get_peer_addr_str(WFHttpTask *server_task);
};


#endif // _HTTPUTIL_H_