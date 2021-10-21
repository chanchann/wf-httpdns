#ifndef _HDSERVICE_H_
#define _HDSERVICE_H_

#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/HttpMessage.h>
#include <spdlog/spdlog.h>
#include <workflow/WFDnsClient.h>
#include <workflow/DnsUtil.h>
#include <vector>

using namespace protocol;

class HDService
{
public:
	static void single_dns_resolve(WFHttpTask *server_task,
								   std::map<std::string, std::string> &query_split);

	static void multi_dns_resolve(WFHttpTask *server_task,
								  std::map<std::string, std::string> &query_split);

};

#endif // _HDSERVICE_H_