#ifndef CONTEXT_H
#define CONTEXT_H

#include <vector>
#include <string>
#include <mutex>
#include <workflow/WFTaskFactory.h>
#include "json.hpp"

using json = nlohmann::json;
/*
single resp : 
{
    "host": "www.baidu.com",
    "ips": [
        "220.181.38.149",
        "220.181.38.150"
    ],
    "ttl": 42,
    "origin_ttl": 300,    
    "client_ip": "36.110.147.196"
}
*/

struct single_dns_context
{
    json js;
    WFHttpTask *server_task;
};

/*
multi resp : 
{
    "dns": [
        {
            "host": "www.sogou.com",
            "client_ip": "36.110.147.196",
            "ips": [],
            "type": 1,
            "ttl": 300
        },
        {
            "host": "www.baidu.com",
            "client_ip": "36.110.147.196",
            "ips": [
                "220.181.38.150",
                "220.181.38.149"
            ],
            "type": 1,
            "ttl": 171,
            "origin_ttl": 300
        }
    ]
}
*/

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