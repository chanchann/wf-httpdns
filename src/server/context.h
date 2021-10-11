#include <vector>
#include <string>
#include <workflow/WFTaskFactory.h>
#include "json.hpp"

using json = nlohmann::json;
/*
resp : 
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

// todo : 1. how to get origin_ttl 

struct series_context
{
    json js;
    WFHttpTask *server_task;
};