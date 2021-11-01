#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <workflow/WFHttpServer.h>
#include <workflow/URIParser.h>
#include <spdlog/spdlog.h>
#include <csignal>
#include <unordered_map>
#include "HDService.h"

using namespace protocol;

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    signal(SIGINT, sig_handler);

    struct WFGlobalSettings settings = GLOBAL_SETTINGS_DEFAULT;
    settings.dns_ttl_default = 300;
    settings.dns_ttl_min = 60;
    WORKFLOW_library_init(&settings);

    spdlog::set_level(spdlog::level::trace);

    WFGlobal::get_dns_client()->init("dns://119.29.29.29/");

    WFHttpServer server([](WFHttpTask *server_task)
    {
        const char *request_uri = server_task->get_req()->get_request_uri();
        const char *cur = request_uri;
        while (*cur && *cur != '?')
            cur++;

        std::string path(request_uri, cur - request_uri);
        std::string query(cur + 1);

        auto query_split = URIParser::split_query(query);

        if (strcmp(path.c_str(), "/d") == 0)
        {
            HDService::single_dns_resolve(server_task, query_split);
            return;
        } else if (strcmp(path.c_str(), "/resolve") == 0)
        {
            HDService::batch_dns_resolve(server_task, query_split);
            return;
        } else
        {
            server_task->get_resp()->append_output_body_nocopy(R"({"code": "Invalid Query"})", 25);
            return;
        }
    });

    if (server.start(9001) == 0)
    {
        wait_group.wait();
        server.stop();
        WFGlobal::get_dns_client()->deinit();
    }

    return 0;
}