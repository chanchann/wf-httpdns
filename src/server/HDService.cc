#include "HDService.h"
#include "HDFactory.h"
#include "Util.h"
#include "Context.h"

std::string HDService::check_host_field(WFHttpTask *server_task,
                                        std::map<std::string, std::string> &query_split)
{
    auto host_list = StringUtil::split(query_split["host"], ',');
    HttpResponse *server_resp = server_task->get_resp();
    if (host_list.empty())
    {
        server_resp->append_output_body_nocopy(R"("code": "MissingArgument")", 25);
        spdlog::error("Missing : host field is required");
        return "";
    }
    else if (host_list.size() > 1)
    {
        server_resp->append_output_body_nocopy(R"({"code": "TooManyHosts"})", 24);
        spdlog::error("Too many hosts");
        return "";
    }
    return host_list.at(0);
}

void HDService::check_query_field(SingleDnsCtx *sin_ctx,
                                  std::map<std::string, std::string> &query_split) 
{
    if(query_split.find("query") != query_split.end())
    {   
        auto query_ipvx_list = StringUtil::split(query_split["query"], ',');
        for(auto& ipvx : query_ipvx_list) 
        {
            if(ipvx == "4") 
            {
                sin_ctx->ipv4 = true;
            }
            if(ipvx == "6")
            {
                sin_ctx->ipv6 = true;
            }
        }
    }
    else 
    {
        sin_ctx->ipv4 = true;  // default
    }
}

bool HDService::get_dns_cache(WFHttpTask *server_task,
                                           const std::string &url) 
{   
    spdlog::info("get dns cache url : {}", url);
    auto *sin_ctx = static_cast<SingleDnsCtx *>(server_task->user_data);
    // www.baidu.com[:80]
    auto host_port = StringUtil::split(url, ':');
    if(host_port.size() == 0)
    {
        // In fact, here has already been checked by `check_host_field`
        spdlog::error("url is invalid");
        return false;
    } 
    else if(host_port.size() == 2) 
    {
        sin_ctx->host = host_port[0];
        sin_ctx->port = static_cast<unsigned int>(std::stoul(host_port[1]));
    } 
    else 
    {
        sin_ctx->host = host_port[0];
        sin_ctx->port = 80;   // default http port 80
    }
    auto *dns_cache = WFGlobal::get_dns_cache();
    auto *addr_handle = dns_cache->get(sin_ctx->host, sin_ctx->port);
    if(addr_handle) 
    {
        spdlog::info("get cache");
        struct addrinfo *addrinfo = addr_handle->value.addrinfo;
        sin_ctx->ttl = addr_handle->value.expire_time;
        char ip_str[128];
        do {
            if(addrinfo->ai_family == AF_INET && sin_ctx->ipv4)
            {
                inet_ntop(AF_INET, &(((struct sockaddr_in *)addrinfo->ai_addr)->sin_addr), ip_str, 128);
                sin_ctx->ips.push_back(ip_str);
            }
            else if(addrinfo->ai_family == AF_INET6 && sin_ctx->ipv6)
            {
                inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)addrinfo->ai_addr)->sin6_addr), ip_str, 128);
                sin_ctx->ipsv6.push_back(ip_str);
            }
            addrinfo = addrinfo->ai_next;
        } while(addrinfo);

        to_json(sin_ctx->js, *sin_ctx);
        spdlog::info("from cache : {}", sin_ctx->js.dump());
        server_task->get_resp()->append_output_body(sin_ctx->js.dump());

        dns_cache->release(addr_handle);
    }
    else 
    {
        spdlog::info("no cache");
        return false;
    }
    return true;
}

static inline void __parallel_callback(const ParallelWork *pwork)
{
    auto *server_task = static_cast<WFHttpTask *>(pwork->get_context());
    auto sin_ctx = 
        static_cast<SingleDnsCtx *>(server_task->user_data);

    // put cache
    auto *dns_cache = WFGlobal::get_dns_cache();
    const auto *settings = WFGlobal::get_global_settings();
    unsigned int dns_ttl_default = settings->dns_ttl_default;
    unsigned int dns_ttl_min = settings->dns_ttl_min;
    const DnsCache::DnsHandle *addr_handle;
    addr_handle = dns_cache->put(sin_ctx->host, 
                                sin_ctx->port, 
                                sin_ctx->addrinfo,
                                dns_ttl_default,
                                dns_ttl_min);
    dns_cache->release(addr_handle);

    HttpResponse *server_resp = server_task->get_resp();
    to_json(sin_ctx->js, *sin_ctx);
    spdlog::info("res : {}", sin_ctx->js.dump());
    server_resp->append_output_body(sin_ctx->js.dump());
    spdlog::info("All series in this parallel have done");
}

void HDService::single_dns_resolve(WFHttpTask *server_task,
                                   std::map<std::string, std::string> &query_split)
{
    spdlog::trace("single dns request");

    auto *dns_client = WFGlobal::get_dns_client();
    std::string host = check_host_field(server_task, query_split);
    if(host.empty()) return;
    
    spdlog::info("Recv request {}", host);

    SingleDnsCtx *sin_ctx = new SingleDnsCtx;
    sin_ctx->server_task = server_task;
    sin_ctx->client_ip = Util::get_peer_addr_str(server_task);

    server_task->user_data = sin_ctx;

    check_query_field(sin_ctx, query_split);

    if(get_dns_cache(server_task, host)) return;
    
    // if no cache
    auto pwork = Workflow::create_parallel_work(__parallel_callback);
    pwork->set_context(server_task);

    if(sin_ctx->ipv4) 
    {
        spdlog::trace("add ipv4 series");
        auto dns_task_v4 = HDFactory::create_dns_task(host);
        dns_task_v4->user_data = sin_ctx;
        pwork->add_series(Workflow::create_series_work(dns_task_v4, nullptr));
    }
    if(sin_ctx->ipv6)
    {
        spdlog::trace("add ipv6 series");
        auto dns_task_v6 = HDFactory::create_dns_task(host);
        dns_task_v6->get_req()->set_question_type(DNS_TYPE_AAAA);
        dns_task_v6->user_data = sin_ctx;
        pwork->add_series(Workflow::create_series_work(dns_task_v6, nullptr));   
    }
    
    server_task->set_callback([](WFHttpTask *server_task) {
        spdlog::info("server task done");
        delete static_cast<SingleDnsCtx *>(server_task->user_data);
    });

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

WFGraphTask* HDService::build_task_graph(WFHttpTask *server_task,
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
    auto *graph = build_task_graph(server_task, query_split);
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





