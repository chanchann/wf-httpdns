#ifndef _CONTEXT_H_
#define _CONTEXT_H_

#include <vector>
#include <string>
#include <mutex>
#include <workflow/WFTaskFactory.h>
#include "json.hpp"

using json = nlohmann::json;

struct SingleDnsCtx
{
    std::string host;
    std::vector<std::string> ips;
    std::vector<std::string> ipsv6;
    int ttl;
    int origin_ttl;               
    std::string client_ip;
    
    WFHttpTask *server_task;
    struct addrinfo *addrinfo;
    bool ipv4;
    bool ipv6;
    json js;
};

static inline void to_json(json &js, const SingleDnsCtx &dns_ctx)
{
    js = json {
        {"host", dns_ctx.host},
        {"ips", dns_ctx.ips},
        {"ipsv6", dns_ctx.ipsv6},   
        {"ttl", dns_ctx.ttl},
        {"origin_ttl", dns_ctx.origin_ttl},
        {"client_ip", dns_ctx.client_ip}
    };
}

struct DnsCtx
{
    std::string host;
    std::vector<std::string> ips;
    int ttl;
    int origin_ttl;                 
    std::string client_ip;
    WFHttpTask *server_task;
};

static inline void to_json(json &js, const DnsCtx &dns_ctx)
{
    js = json {
        {"host", dns_ctx.host},
        {"ips", dns_ctx.ips},
        {"ttl", dns_ctx.ttl},
        {"origin_ttl", dns_ctx.origin_ttl},
        {"client_ip", dns_ctx.client_ip}
    };
}

struct GatherCtx
{
    json js;
    std::vector<DnsCtx *> dns_ctx_gather_list;
    std::mutex mutex;
    bool ipv4;
    bool ipv6;
    
    std::vector<std::string> not_in_cache_v4;
    std::vector<std::string> not_in_cache_v6;
};



#endif // _CONTEXT_H_
