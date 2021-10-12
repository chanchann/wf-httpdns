# wf-httpdns

HttpDns based on workflow

workflow project : https://github.com/sogou/workflow

## Single domain name resolution interface

HTTPDNS provides external domain name resolution services through the HTTP interface. The service access directly uses the server's IP address. 

Request method: HTTP GET or HTTPS GET

HTTP service URL: http://127.0.0.1/d

HTTPS service URL: https://127.0.0.1/d

URL parameter description:


|  name   |    | description   |
|  ----  | ----  | ----|
| host  | Required | Domain name to be resolved |
| query  | optional | Specify the type of the resolved IP, you can choose 6 (IPv6) or 4 (IPv4).The default value is 4.  |

When accessing the HTTPDNS service, only one domain name can be resolved at a time.

Example request:

Example 1: http://127.0.0.1/d?host=www.baidu.com

Example 2 (Specify the resolution type): http://127.0.0.1/d?host=www.baidu.com&query=4,6
