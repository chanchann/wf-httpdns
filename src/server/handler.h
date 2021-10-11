#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/HttpMessage.h>
#include <spdlog/spdlog.h>
#include <workflow/WFDnsClient.h>
#include <workflow/DnsUtil.h>
#include <vector>
#include "json.hpp"

using json = nlohmann::json;
using namespace protocol;

void dnsHandler(WFHttpTask *server_task, WFDnsClient& client, std::string& url) {
	auto dns_task = create_dns_task(client, url);
	**server_task << dns_task;

	series_context *context = new series_context;
	// add host part
	context->host = std::move(url);    
	
	server_task->set_callback([&context](WFHttpTask *server_task) {
		json js;
		js["host"] = context->host;
		js["ips"] = context->ips;
		js["ttl"] = context->ttl;
		js["client_ip"] = context->client_ip;
		// HttpResponse *resp = server_task->get_resp();
		// resp->append_output_body(js.dump());
		spdlog::info("dns query finished, state = {}", server_task->get_state());
	});
	
	SeriesWork *series = series_of(server_task);
	series->set_context(context);
	series->set_callback([](const SeriesWork *series)
	{
		spdlog::trace("delete context");
		delete static_cast<series_context *>(series->get_context());
	});
	
	// add client_ip part
	char addrstr[128];
	struct sockaddr_storage addr;
	socklen_t addr_len = sizeof addr;
	unsigned short port = 0;
	server_task->get_peer_addr((struct sockaddr *)&addr, &addr_len);
	if (addr.ss_family == AF_INET)
	{
		struct sockaddr_in *sin = (struct sockaddr_in *)&addr;
		inet_ntop(AF_INET, &sin->sin_addr, addrstr, 128);
		port = ntohs(sin->sin_port);
	}
	else if (addr.ss_family == AF_INET6)
	{
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&addr;
		inet_ntop(AF_INET6, &sin6->sin6_addr, addrstr, 128);
		port = ntohs(sin6->sin6_port);
	}
	else
		strcpy(addrstr, "Unknown");

	context->client_ip = addrstr;
}