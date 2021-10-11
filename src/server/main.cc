#include <iostream>
#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <workflow/WFHttpServer.h>
#include <workflow/WFDnsClient.h>
#include <workflow/URIParser.h>
#include <spdlog/spdlog.h>
#include <memory>
#include <signal.h>
#include <unordered_map>
#include "factory.h"
#include "handler.h"

using namespace protocol;

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    signal(SIGINT, sig_handler);
    WFDnsClient client;
    client.init("dns://8.8.8.8/");

    WFHttpServer server([&client](WFHttpTask *server_task)
    {
        const char *request_uri = server_task->get_req()->get_request_uri();
        auto query_split = URIParser::split_query(request_uri);
        auto first_pair = query_split.begin();

        if (first_pair != query_split.end() && strcmp(first_pair->first.c_str(), "/d") == 0)
        {
            dnsReqHandler(server_task, client, first_pair->second);
            return;
        }
        else
        {
            server_task->get_resp()->append_output_body("Invalid query");
            return;
        }
    });

    if (server.start(8888) == 0)
    {
        wait_group.wait();
        client.deinit();
        server.stop();
    }

    return 0;
}