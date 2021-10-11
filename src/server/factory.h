#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/HttpMessage.h>
#include <spdlog/spdlog.h>
#include <workflow/WFDnsClient.h>
#include <workflow/DnsUtil.h>
#include <vector>
#include "util.h"
#include "context.h"

using namespace protocol;

WFDnsTask *create_dns_task(WFDnsClient &dnsClient, const std::string &url)
{
	spdlog::info("create dns task");

	WFDnsTask *dns_task = dnsClient.create_dns_task(url,
		[](WFDnsTask *dns_task)
		{
			if (dns_task->get_state() == WFT_STATE_SUCCESS)
			{
				auto dns_resp = dns_task->get_resp();
				DnsResultCursor cursor(dns_resp);
				dns_record *record = nullptr;
				while (cursor.next(&record))
				{
					spdlog::info("name : {}", record->name);
					spdlog::info("rdlength : {}", record->rdlength);
					if (record->type == DNS_TYPE_A)
					{
                        auto series = series_of(dns_task);
						series_context *context =
							static_cast<series_context *>(series->get_context());
                        // add ip_list/ttl part
						context->ips.emplace_back(ipBinToString(record->rdata)); 
                        context->ttl = record->ttl;
					} 
				}
			}

		});
	return dns_task;
}
