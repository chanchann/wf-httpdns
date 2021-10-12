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

Example 2 (Specify the resolution type): http://127.0.0.1:9001/d?host=www.baidu.com&query=4,6

- Request succeed

When the request is successful, the HTTP response status code is 200, and the response result is expressed in JSON format. An example is as follows:

```
{
    "client_ip": "172.17.0.1",
    "host": "www.baidu.com",
    "ips": [
        "14.215.177.38",
        "14.215.177.39"
    ],
    "ttl": 39
}
```

## Batch domain name resolution interface

Request method: HTTP GET or HTTPS GET

Service URL: http://127.0.0.1:9001/{account_id}/resolve

URL parameter description:


|  name   |    | description   |
|  ----  | ----  | ----|
| host  | Required | Domain name to be resolved. Multiple domain names are separated by a comma, and a single request is allowed to carry up to 5 domain names.|

- Example 

Example 1: http://127.0.0.1:9001/resolve?host=www.baidu.com,www.tencent.com

- Request succeed

```
{
    "dns": [
        {
            "client_ip": "172.17.0.1",
            "host": "www.baidu.com",
            "ips": [
                "14.215.177.38",
                "14.215.177.39"
            ],
            "origin_ttl": 0,
            "ttl": 254
        },
        {
            "client_ip": "172.17.0.1",
            "host": "www.tencent.com",
            "ips": [
                "211.152.148.99",
                "211.152.148.87",
                "211.152.148.77",
                "211.152.148.72",
                "211.152.148.44",
                "211.152.148.29",
                "211.152.148.84",
                "211.152.148.30",
                "211.152.148.78",
                "211.152.149.16"
            ],
            "origin_ttl": 0,
            "ttl": 60
        }
    ]
}
```