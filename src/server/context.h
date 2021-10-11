#include <vector>
#include <string>

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

// todo : origin_ttl 

struct series_context
{
    std::string host;
    std::vector<std::string> ips;
    int ttl;
    int origin_ttl;
    std::string client_ip;
};