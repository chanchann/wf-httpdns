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
        const char *cur = request_uri;
        while (*cur && *cur != '?')
            cur++;

        std::string path(request_uri, cur - request_uri);
        std::string query(cur+1); 

        auto query_split = URIParser::split_query(query);

        if (strcmp(path.c_str(), "/d") == 0)
        {
            singleDnsReq(server_task, client, query_split);
            return;
        }
        else if(strcmp(path.c_str(), "/resolve") == 0)
        {
            multiDnsReq(server_task, client, query_split);
            return;
        }
        else
        {
            server_task->get_resp()->append_output_body_nocopy(R"({"code": "Invalid Query"})", 25);
            return;
        }
    });

    if (server.start(9001) == 0)
    {
        wait_group.wait();
        server.stop();
        client.deinit();
    }

    return 0;
}