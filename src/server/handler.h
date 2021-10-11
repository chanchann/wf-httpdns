#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/HttpMessage.h>
#include <spdlog/spdlog.h>
#include <workflow/WFDnsClient.h>
#include <workflow/DnsUtil.h>
// #include "util.h"

using namespace protocol;

WFDnsTask* create_dns_task(WFDnsClient *dnsClient, const std::string &url)
{
	spdlog::info("create dns task");

	WFDnsTask *dns_task = dnsClient->create_dns_task(url,
		[](WFDnsTask *dns_task)
		{	
			spdlog::info("1");
			if (dns_task->get_state() == WFT_STATE_SUCCESS)
			{
				spdlog::info("2");
				auto dns_resp = dns_task->get_resp();
				DnsResultCursor cursor(dns_resp);
				dns_record* record = nullptr;
				while(cursor.next(&record)) {
					spdlog::info("{}", record->rdlength);
					// if(record->type == DNS_TYPE_A) {
					// 	spdlog::info("{}", ipBinToString(record->rdata));
					// }

					// spdlog::info("{}", static_cast<char*>(record->rdata));
				}
			}
		});


}

