#include <iostream>
#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFFacilities.h>
#include <workflow/WFHttpServer.h>
#include <spdlog/spdlog.h>
#include <memory>
#include <signal.h>
#include <unordered_map>

using namespace protocol;

void process(WFHttpTask *server_task) {
    server_task->set_callback([](WFHttpTask *task) {
        spdlog::info("httpdns task finished, state = {}", task->get_state());
    });
    if (strcmp(server_task->get_req()->get_request_uri(), "/d") == 0)
    {
        
    }
}

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    signal(SIGINT, sig_handler);
    WFHttpServer server(process);
    
    if (server.start(8888) == 0) {
        wait_group.wait();
        server.stop();
    }

    return 0;
}