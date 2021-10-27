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
		create_dns_task(const std::string &host, bool is_batch = false);

	static void start_dns_paralell(WFHttpTask *server_task, 
												bool ipv4 = true);

private:
	static SeriesWork *create_dns_series(ParallelWork *pwork, 
										const std::string &host);
};

#endif // _HDFACTORY_H_