#ifndef _UTIL_H_
#define _UTIL_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string>
#include <workflow/WFTaskFactory.h>
#include <workflow/StringUtil.h>

class Util
{
public:
    static std::string ip_bin_to_str(void *sa_sin_addr, bool ipv4 = true);

    static std::string get_peer_addr_str(WFHttpTask *server_task);

    template<typename T>
    static bool split_host_port(T *ctx, const std::string& url);
};


template<typename T>
bool Util::split_host_port(T *ctx, const std::string& url) 
{
    // www.baidu.com[:80]
    auto host_port = StringUtil::split(url, ':');
    if(host_port.size() == 0)
    {
        // In fact, here has already been checked by `check_host_field`
        return false;
    } 
    else if(host_port.size() == 2) 
    {
        ctx->host = host_port[0];
        ctx->port = static_cast<unsigned int>(std::stoul(host_port[1]));
    } 
    else 
    {
        ctx->host = host_port[0];
        ctx->port = 80;   // default http port 80
    }
    return true;
}

#endif // _UTIL_H_
