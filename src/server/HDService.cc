#include "HDService.h"
#include "Context.h"
#include "HDFactory.h"
#include "Util.h"

static inline std::string __check_host_field(WFHttpTask *server_task, std::map<std::string, std::string> &query_split) 
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

static inline void __check_query_field(bool &ipv4, bool &ipv6, std::map<std::string, std::string> &query_split)
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

static inline SingleDnsCtx* __get_dns_cache(WFHttpTask *server_task, 
                                        const std::string& url)
{   
    // www.baidu.com[:80]
    auto host_port = StringUtil::split(url, ':');
    std::string host;
    unsigned int port;
    if(host_port.size() == 0)
    {
        spdlog::error("url is invalid");
        return nullptr;
    } 
    else if(host_port.size() == 2) 
    {
        host = host_port[0];
        port = static_cast<unsigned int>(std::stoul(host_port[1]));
    } 
    else 
    {
        port = 80;   // 暂时先填这个端口
    }
    SingleDnsCtx *sin_ctx;
    auto *dns_cache = WFGlobal::get_dns_cache();
    auto *addr_handle = dns_cache->get(host, port);
    if(addr_handle) 
    {
        spdlog::info("get cache");
        sin_ctx = new SingleDnsCtx;
        struct addrinfo *addrinfo = addr_handle->value.addrinfo;
        sin_ctx->client_ip = Util::get_peer_addr_str(server_task);
        sin_ctx->host = host;
        sin_ctx->ttl = addr_handle->value.expire_time;
        char ip_str[128];
        inet_ntop(AF_INET, &(((struct sockaddr_in *)addrinfo->ai_addr)->sin_addr), ip_str, 128);
        // inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)addrinfo->ai_addr)->sin6_addr), buf, 128);
        sin_ctx->ips.push_back(ip_str);

        dns_cache->release(addr_handle);
    }
    else 
    {
        spdlog::info("no cache");
        return nullptr;
    }
    server_task->user_data = sin_ctx;
    return sin_ctx;
}

void HDService::single_dns_resolve(WFHttpTask *server_task,
                                   std::map<std::string, std::string> &query_split)
{
    auto *dns_client = WFGlobal::get_dns_client();
    spdlog::trace("single dns request");
    std::string host = __check_host_field(server_task, query_split);
    if(host.empty()) return;

    spdlog::info("Recv request {}", host);

    bool ipv4 = false, ipv6 = false;
    __check_query_field(ipv4, ipv6, query_split);

    auto *dns_ctx = __get_dns_cache(server_task, host);
    if(dns_ctx)
    {
        json js;
        to_json(js, *dns_ctx);
        // spdlog::info("from cache : {}", js.dump());
        server_task->get_resp()->append_output_body(js.dump());

        return;
    }

    // if no cache
    auto pwork = Workflow::create_parallel_work([](const ParallelWork *pwork)
    {
        auto *server_task = static_cast<WFHttpTask *>(pwork->get_context());
        auto sin_ctx = 
            static_cast<SingleDnsCtx *>(server_task->user_data);
        HttpResponse *server_resp = server_task->get_resp();
        json js;
        to_json(js, *sin_ctx);
        // spdlog::info("res : {}", js.dump());
		server_resp->append_output_body(js.dump());
        spdlog::info("All series in this parallel have done");
    });
    pwork->set_context(server_task);

    if(ipv4) 
    {
        spdlog::trace("add ipv4 series");
        auto dns_task_v4 = HDFactory::create_dns_task(host);
        dns_task_v4->user_data = server_task;
        pwork->add_series(Workflow::create_series_work(dns_task_v4, nullptr));
    }
    if(ipv6)
    {
        spdlog::trace("add ipv6 series");
        auto dns_task_v6 = HDFactory::create_dns_task(host);
        dns_task_v6->get_req()->set_question_type(DNS_TYPE_AAAA);
        dns_task_v6->user_data = server_task;
        pwork->add_series(Workflow::create_series_work(dns_task_v6, nullptr));   
    }
    
    server_task->set_callback([](WFHttpTask *server_task) {
        delete static_cast<SingleDnsCtx *>(server_task->user_data);
        spdlog::info("server task done");
    });
    
    SingleDnsCtx *sin_ctx = new SingleDnsCtx;
    sin_ctx->server_task = server_task;
    sin_ctx->host = std::move(host);
    sin_ctx->client_ip = Util::get_peer_addr_str(server_task);

    server_task->user_data = sin_ctx;

    **server_task << pwork;
}

static inline void __graph_callback(const WFGraphTask *graph)
{
    auto *server_task = static_cast<WFHttpTask *>(graph->user_data);
    auto *gather_ctx = static_cast<GatherCtx *>(server_task->user_data);
    auto gather_list = gather_ctx->dns_ctx_gather_list;
    json dns_js;
    for (auto dns_ctx : gather_list)
    { 
        to_json(dns_js, *dns_ctx);
        gather_ctx->js["dns"].push_back(dns_js);
    }
    server_task->get_resp()->append_output_body(gather_ctx->js.dump());
    spdlog::info("{}", gather_ctx->js.dump());

    spdlog::info("graph task done");
}

static inline WFGraphTask* __build_task_graph(WFHttpTask *server_task,
                                    std::map<std::string, std::string> &query_split)
{
    spdlog::trace("build task graph");
    bool ipv4 = false, ipv6 = false;
    __check_query_field(ipv4, ipv6, query_split);

    WFGraphTask *graph = WFTaskFactory::create_graph_task(__graph_callback);

    graph->user_data = server_task;
    if(ipv4)
    {
        ParallelWork *pwork_v4 = HDFactory::create_dns_paralell(server_task, query_split);
        if (!pwork_v4) return nullptr;

        WFGraphNode& ipv4_node = graph->create_graph_node(pwork_v4);
        spdlog::trace("connect ipv4 node");
    }
    if(ipv6)
    {
        ParallelWork *pwork_v6 = HDFactory::create_dns_paralell(server_task, query_split, false);
        if (!pwork_v6) return nullptr;
        
        WFGraphNode& ipv6_node = graph->create_graph_node(pwork_v6);
        spdlog::trace("connect ipv6 node");
    }	
    return graph;
}

void HDService::multi_dns_resolve(WFHttpTask *server_task,
                                  std::map<std::string, std::string> &query_split)
{
    spdlog::trace("multiple dns request");
    auto *graph = __build_task_graph(server_task, query_split);
    if(!graph)
    {
        server_task->get_resp()->append_output_body_nocopy(R"({"code": "host field is required"})", 34);
        return;
    }
    **server_task << graph;

    GatherCtx *gather_ctx = new GatherCtx;
    server_task->user_data = gather_ctx;

    server_task->set_callback([](WFHttpTask *server_task)
    {
        auto *gather_ctx = static_cast<GatherCtx *>(server_task->user_data);
        for(auto* dns_ctx : gather_ctx->dns_ctx_gather_list)
        {
            delete dns_ctx;
        }
        delete gather_ctx;
        spdlog::trace("delete dns_ctx and gather ctx");
        spdlog::info("server task done");
    });
}