#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/HttpMessage.h>
#include <spdlog/spdlog.h>
#include <workflow/WFDnsClient.h>
#include <workflow/DnsUtil.h>
#include <vector>

using namespace protocol;

void dnsReqHandler(WFHttpTask *server_task, WFDnsClient& client, std::string& url) {
	spdlog::info("Recv request {}", url);
	auto dns_task = create_dns_task(client, url);
	**server_task << dns_task;

	series_context *context = new series_context;
	context->server_task = server_task;
	series_of(server_task)->set_context(context);

	context->js["host"] = std::move(url);    
	
	server_task->set_callback([](WFHttpTask *server_task) {
		delete static_cast<series_context *>(series_of(server_task)->get_context());
		spdlog::info("dns query finished and send back to client, state = {}", server_task->get_state());
	});

	context->js["client_ip"] = getPeerAddrStr(server_task);
}