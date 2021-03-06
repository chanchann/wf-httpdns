# wf-httpdns

HttpDns based on workflow

workflow project : https://github.com/sogou/workflow

## Single domain name resolution interface

HTTPDNS provides external domain name resolution services through the HTTP interface. The service access directly uses the server's IP address. 

- Request method: HTTP GET or HTTPS GET

HTTP service URL: http://127.0.0.1:9001/d

HTTPS service URL: http://127.0.0.1:9001/d

URL parameter description:


|  name   |    | description   |
|  ----  | ----  | ----|
| host  | Required | Domain name to be resolved |
| query  | optional | Specify the type of the resolved IP, you can choose 6 (IPv6) or 4 (IPv4).The default value is 4.  |

When accessing the HTTPDNS service, only one domain name can be resolved at a time.

Example request:

Example 1: http://127.0.0.1:9001/d?host=www.baidu.com

Example 2 (Specify the resolution type): http://127.0.0.1:9001/d?host=ipv6.tsinghua.edu.cn&query=4,6

- Request succeed

When the request is successful, the HTTP response status code is 200, and the response result is expressed in JSON format. An example is as follows:


```
http://127.0.0.1:9001/d?host=ipv6.tsinghua.edu.cn

{
    "host": "ipv6.tsinghua.edu.cn",
    "ips": [
        "166.111.8.205"
    ],
    "ttl": 21369,
    "origin_ttl": 21600,
    "client_ip": "118.199.22.176"
}
```

```
http://127.0.0.1:9001/d?host=www.baidu.com&query=4,6

{
    "ipsv6": [],
    "host": "www.baidu.com",
    "client_ip": "118.199.22.176",
    "ips": [
        "182.61.200.7",
        "182.61.200.6"
    ],
    "ttl": 300,
    "origin_ttl": 300
}
```

```
http://127.0.0.1:9001/d?host=ipv6.tsinghua.edu.cn&query=4,6

{
    "client_ip": "172.17.0.1",
    "host": "ipv6.tsinghua.edu.cn",
    "ips": [
        "166.111.8.205"
    ],
    "ipsv6": [
        "2402:f000:1:881::8:205"
    ],
    "ttl": 7315
}
```



## Batch domain name resolution interface

Request method: HTTP GET or HTTPS GET

Service URL: http://127.0.0.1:9001/resolve

URL parameter description:


|  name   |    | description   |
|  ----  | ----  | ----|
| host  | Required | Domain name to be resolved. Multiple domain names are separated by a comma, and a single request is allowed to carry up to 5 domain names.|

- Example 

Example 1: http://127.0.0.1:9001/resolve?host=www.baidu.com,ipv6.tsinghua.edu.cn

```
{
    "dns": [
        {
            "host": "www.baidu.com",
            "client_ip": "106.121.166.99",
            "ips": [
                "220.181.38.150",
                "220.181.38.149"
            ],
            "type": 1,
            "ttl": 26,
            "origin_ttl": 300
        },
        {
            "host": "ipv6.tsinghua.edu.cn",
            "client_ip": "106.121.166.99",
            "ips": [
                "166.111.8.205"
            ],
            "type": 1,
            "ttl": 21037,
            "origin_ttl": 21600
        }
    ]
}
```

Example 2: http://127.0.0.1:9001/resolve?host=www.baidu.com,ipv6.tsinghua.edu.cn&query=4,6

```
{
    "dns": [
        {
            "host": "www.baidu.com",
            "client_ip": "106.121.166.99",
            "ips": [
                "220.181.38.149",
                "220.181.38.150"
            ],
            "type": 1,
            "ttl": 295,
            "origin_ttl": 300
        },
        {
            "host": "www.baidu.com",
            "client_ip": "106.121.166.99",
            "ips": [],
            "type": 28,
            "ttl": 295
        },
        {
            "host": "ipv6.tsinghua.edu.cn",
            "client_ip": "106.121.166.99",
            "ips": [
                "166.111.8.205"
            ],
            "type": 1,
            "ttl": 20996,
            "origin_ttl": 21600
        },
        {
            "host": "ipv6.tsinghua.edu.cn",
            "client_ip": "106.121.166.99",
            "ips": [
                "2402:f000:1:881:0:0:8:205"
            ],
            "type": 28,
            "ttl": 21006,
            "origin_ttl": 21600
        }
    ]
}
```