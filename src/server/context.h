#ifndef CONTEXT_H
#define CONTEXT_H

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

struct dns_context
{
    std::string host;
    std::vector<std::string> ips;
    int ttl;
    int origin_ttl;  // todo : 1. how to get origin_ttl
    std::string client_ip;
};

void to_json(json& js, const dns_context& dns_ctx) {
    js = json {
        {"host", dns_ctx.host}, 
        {"ips", dns_ctx.ips}, 
        {"ttl", dns_ctx.ttl},
        {"origin_ttl", dns_ctx.origin_ttl},
        {"client_ip", dns_ctx.client_ip}
    };
}

struct parallel_context
{
    json js;
    WFHttpTask *server_task;
    std::vector<dns_context *> dns_context_list;
    std::mutex mutex;
};


#endif // CONTEXT_H