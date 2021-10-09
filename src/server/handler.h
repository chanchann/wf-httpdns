#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/HttpMessage.h>
#include <spdlog/spdlog.h>
#include <workflow/WFDnsClient.h>

using namespace protocol;

void create_dns_task(WFHttpTask *server_task, WFDnsClient *dnsClient, std::string &url)
{
	spdlog::info("create dns task");
	HttpRequest *http_req = server_task->get_req();

	spdlog::info("{}", http_req->get_request_uri());
	WFDnsTask *dns_task = dnsClient->create_dns_task(url,
		[&server_task](WFDnsTask *dns_task)
		{
			int state = dns_task->get_state();

			if (state == WFT_STATE_SUCCESS)
			{
				auto dns_resp = dns_task->get_resp();
				dns_resp->get_question_type();
				spdlog::info("type : {}", dns_resp->get_question_name());
			}
		});

	**server_task << dns_task;
}
