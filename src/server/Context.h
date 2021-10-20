#ifndef _CONTEXT_H_
#define _CONTEXT_H_

#include <vector>
#include <string>
#include <mutex>
#include <workflow/WFTaskFactory.h>
#include "json.hpp"

using json = nlohmann::json;

struct single_dns_context
{
    json js;
    WFHttpTask *server_task;
};

struct parallel_context;

struct dns_context
{
    std::string host;
    std::vector<std::string> ips;
    int ttl;
    int origin_ttl; // todo : 1. how to get origin_ttl
    std::string client_ip;
    parallel_context *para_ctx;  // 串到上一层
};

static inline void to_json(json &js, const dns_context &dns_ctx)
{
    js = json{
        {"host", dns_ctx.host},
        {"ips", dns_ctx.ips},
        {"ttl", dns_ctx.ttl},
        {"origin_ttl", dns_ctx.origin_ttl},
        {"client_ip", dns_ctx.client_ip}};
}

// for ipv4 / ipv6 parallel
struct parallel_context
{
    std::vector<dns_context *> dns_context_list;
    std::mutex mutex;
    bool ipv4 = true;
    WFHttpTask *server_task;    // 串到上一层
};

struct gather_context
{
    json js;
    std::vector<dns_context *> dns_ctx_gather_list;
    std::mutex mutex;
};


#endif // _CONTEXT_H_
