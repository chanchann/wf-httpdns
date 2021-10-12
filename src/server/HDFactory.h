#ifndef _HDFACTORY_H_
#define _HDFACTORY_H_

#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/HttpMessage.h>
#include <spdlog/spdlog.h>
#include <workflow/WFDnsClient.h>
#include <workflow/DnsUtil.h>
#include <workflow/StringUtil.h>
#include <mutex>
#include <vector>

using namespace protocol;

class HDFactory
{
public:
	static WFDnsTask *
	create_dns_task(WFDnsClient &dnsClient,
					const std::string &url,
					bool mutli = false);

	static ParallelWork *
	create_dns_paralell(WFDnsClient &dnsClient,
						std::map<std::string, std::string> &query_split);

private:
	static SeriesWork *
	create_dns_series(WFDnsClient &dnsClient, const std::string &host);
};

#endif // _HDFACTORY_H_