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
			spdlog::info("request DNS successfully...");
			series_context *context =
				static_cast<series_context *>(series_of(dns_task)->get_context());
			auto dns_resp = dns_task->get_resp();
			DnsResultCursor cursor(dns_resp);
			dns_record *record = nullptr;
			std::vector<std::string> ips;
			while (cursor.next(&record))
			{
				if (record->type == DNS_TYPE_A)
				{	
					ips.emplace_back(ipBinToStr(record->rdata));
					context->js["ttl"] = record->ttl;
				}
			}
			context->js["ips"] = ips;
			HttpResponse *server_resp = context->server_task->get_resp();
			server_resp->append_output_body(context->js.dump());
		}
	});
	return dns_task;
}

