#include "HDService.h"
#include "Context.h"
#include "HDFactory.h"
#include "Util.h"

std::string __check_host_field(WFHttpTask *server_task, std::map<std::string, std::string> &query_split) 
{
    auto host_list = StringUtil::split(query_split["host"], ',');
    HttpResponse *server_resp = server_task->get_resp();
    if (host_list.empty())
    {
        server_resp->append_output_body_nocopy(R"({"code": "host field is required"})", 34);
        spdlog::error("Missing : host field is required");
        return "";
    }
    else if (host_list.size() > 1)
    {
        server_resp->append_output_body_nocopy(R"({"code": "TooManyHosts"})", 24);
        spdlog::error("Too many hosts");
        return "";
    }
    std::string host = host_list.at(0);
    return host;
}

void __check_query_field(bool &ipv4, bool &ipv6, std::map<std::string, std::string> &query_split)
{
    if(query_split.find("query") != query_split.end())
    {   
        auto query_ipvx_list = StringUtil::split(query_split["query"], ',');
        for(auto& ipvx : query_ipvx_list) 
        {
            if(ipvx == "4") 
            {
                ipv4 = true;
            }
            if(ipvx == "6")
            {
                ipv6 = true;
            }
        }
    }
    else 
    {
        ipv4 = true;
    }
}
void HDService::single_dns_resolve(WFHttpTask *server_task, WFDnsClient &client,
                                   std::map<std::string, std::string> &query_split)
{
    spdlog::trace("single dns request");
    std::string host = __check_host_field(server_task, query_split);
    if(host.empty()) return;

    spdlog::info("Recv request {}", host);

    bool ipv4 = false, ipv6 = false;
    __check_query_field(ipv4, ipv6, query_split);

    auto pwork = Workflow::create_parallel_work([](const ParallelWork *pwork)
    {
        spdlog::info("All series in this parallel have done");
        auto sin_ctx = 
            static_cast<single_dns_context *>(series_of(pwork)->get_context());
        HttpResponse *server_resp = sin_ctx->server_task->get_resp();
        spdlog::info("res : {}", sin_ctx->js.dump());
		server_resp->append_output_body(sin_ctx->js.dump());
    });

    if(ipv4) 
    {
        spdlog::trace("add ipv4 series");
        auto dns_task_v4 = HDFactory::create_dns_task(client, host);
        pwork->add_series(Workflow::create_series_work(dns_task_v4, nullptr));
    }
    if(ipv6)
    {
        spdlog::trace("add ipv6 series");
        auto dns_task_v6 = HDFactory::create_dns_task(client, host);
        dns_task_v6->get_req()->set_question_type(DNS_TYPE_AAAA);
        pwork->add_series(Workflow::create_series_work(dns_task_v6, nullptr));   
    }
    
    server_task->set_callback([](WFHttpTask *server_task) {
        delete static_cast<single_dns_context *>(series_of(server_task)->get_context());
        spdlog::info("server task done");
    });
    
    single_dns_context *sin_ctx = new single_dns_context;
    sin_ctx->server_task = server_task;
    sin_ctx->js["host"] = std::move(host);
    sin_ctx->js["client_ip"] = Util::get_peer_addr_str(server_task);
    series_of(server_task)->set_context(sin_ctx);
    series_of(server_task)->push_back(pwork);
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