#include "HDService.h"
#include "Context.h"
#include "HDFactory.h"
#include "Util.h"

void HDService::single_dns_resolve(WFHttpTask *server_task, WFDnsClient &client,
                                   std::map<std::string, std::string> &query_split)
{
    spdlog::trace("single dns request");

    auto host_list = StringUtil::split(query_split["host"], ',');
    HttpResponse *server_resp = server_task->get_resp();
    if (host_list.empty())
    {
        server_resp->append_output_body_nocopy(R"({"code": "host field is required"})", 34);
        spdlog::error("Missing : host field is required");
        return;
    }
    else if (host_list.size() > 1)
    {
        server_resp->append_output_body_nocopy(R"({"code": "TooManyHosts"})", 24);
        spdlog::error("Too many hosts");
        return;
    }
    std::string host = host_list.at(0);
    spdlog::info("Recv request {}", host);

    auto dns_task = HDFactory::create_dns_task(client, host);
    **server_task << dns_task;

    single_dns_context *single_ctx = new single_dns_context;
    single_ctx->server_task = server_task;
    single_ctx->js["host"] = std::move(host);
    single_ctx->js["client_ip"] = Util::get_peer_addr_str(server_task);
    series_of(server_task)->set_context(single_ctx);

    server_task->set_callback([](WFHttpTask *server_task)
    {
        delete static_cast<single_dns_context *>(series_of(server_task)->get_context());
        spdlog::info("dns query finished and send back to client, state = {}", server_task->get_state());
    });
}

void HDService::multi_dns_resolve(WFHttpTask *server_task, WFDnsClient &dnsClient,
                                  std::map<std::string, std::string> &query_split)
{
    spdlog::trace("multiple dns request");

    ParallelWork *pwork = HDFactory::create_dns_paralell(dnsClient, query_split);
    if (!pwork)
    {
        server_task->get_resp()->append_output_body_nocopy(R"({"code": "host field is required"})", 34);
        return;
    }

    SeriesWork *series = series_of(server_task);

    parallel_context *para_ctx = new parallel_context;
    para_ctx->server_task = server_task;

    series->set_context(para_ctx);
    series->push_back(pwork);

    server_task->set_callback([](WFHttpTask *server_task)
    {
        spdlog::info("delete parallel context");
        delete static_cast<parallel_context *>(series_of(server_task)->get_context());
    });
}