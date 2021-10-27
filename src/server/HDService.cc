#include "HDService.h"
#include "HDFactory.h"
#include "HttpUtil.h"
#include "Context.h"

static const int IP_STR_LEN = 128;

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


bool HDService::get_dns_cache(WFHttpTask *server_task,
                                           const std::string &url) 
{   
    spdlog::info("get dns cache url : {}", url);
    auto *sin_ctx = static_cast<SingleDnsCtx *>(server_task->user_data);

    auto *dns_cache = WFGlobal::get_dns_cache();
    auto *addr_handle = dns_cache->get(sin_ctx->host, 0);
    if(addr_handle) 
    {
        spdlog::info("Get cache");
        struct addrinfo *addrinfo = addr_handle->value.addrinfo;
        sin_ctx->ttl = addr_handle->value.expire_time;
        char ip_str[IP_STR_LEN];
        do {
            if(addrinfo->ai_family == AF_INET && sin_ctx->ipv4)
            {
                inet_ntop(AF_INET, &((reinterpret_cast<struct sockaddr_in *>(addrinfo->ai_addr))->sin_addr), ip_str, IP_STR_LEN);
                sin_ctx->ips.push_back(ip_str);
            }
            else if(addrinfo->ai_family == AF_INET6 && sin_ctx->ipv6)
            {
                inet_ntop(AF_INET6, &((reinterpret_cast<struct sockaddr_in6 *>(addrinfo->ai_addr))->sin6_addr), ip_str, IP_STR_LEN);
                sin_ctx->ipsv6.push_back(ip_str);
            }
            addrinfo = addrinfo->ai_next;
        } while(addrinfo);

        to_json(sin_ctx->js, *sin_ctx);
        spdlog::info("From cache : {}", sin_ctx->js.dump());

        server_task->get_resp()->append_output_body(sin_ctx->js.dump());

        dns_cache->release(addr_handle);
    }
    else 
    {
        spdlog::info("No cache found ...");
        return false;
    }
    return true;
}

static inline void __parallel_callback(const ParallelWork *pwork)
{
    auto *server_task = static_cast<WFHttpTask *>(pwork->get_context());
    auto sin_ctx = 
        static_cast<SingleDnsCtx *>(server_task->user_data);

    to_json(sin_ctx->js, *sin_ctx);
    spdlog::info("res : {}", sin_ctx->js.dump());
    server_task->get_resp()->append_output_body(sin_ctx->js.dump());
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
    sin_ctx->host = host;
    sin_ctx->client_ip = HttpUtil::get_peer_addr_str(server_task);

    server_task->user_data = sin_ctx;

    check_query_field(sin_ctx, query_split);

    if(get_dns_cache(server_task, host)) return;
    
    // if no cache
    auto pwork = Workflow::create_parallel_work(__parallel_callback);
    pwork->set_context(server_task);

    if(sin_ctx->ipv4) 
    {
        spdlog::trace("Add ipv4 series");
        auto dns_task_v4 = HDFactory::create_dns_task(host);
        dns_task_v4->user_data = sin_ctx;
        pwork->add_series(Workflow::create_series_work(dns_task_v4, nullptr));
    }
    if(sin_ctx->ipv6)
    {
        spdlog::trace("Add ipv6 series");
        auto dns_task_v6 = HDFactory::create_dns_task(host);
        dns_task_v6->get_req()->set_question_type(DNS_TYPE_AAAA);
        dns_task_v6->user_data = sin_ctx;
        pwork->add_series(Workflow::create_series_work(dns_task_v6, nullptr));   
    }
    
    server_task->set_callback([](WFHttpTask *server_task) {
        spdlog::info("Server task done ...");
        delete static_cast<SingleDnsCtx *>(server_task->user_data);
    });

    **server_task << pwork;
}

static inline void __graph_callback(const WFGraphTask *graph)
{
    // gather all the info to json here
    auto *server_task = static_cast<WFHttpTask *>(graph->user_data);
    auto *gather_ctx = static_cast<GatherCtx *>(server_task->user_data);
    
    json dns_js;
    for (auto dns_ctx : gather_ctx->dns_ctx_gather_list)
    { 
        to_json(dns_js, *dns_ctx);
        gather_ctx->js["dns"].push_back(dns_js);
    }
    server_task->get_resp()->append_output_body(gather_ctx->js.dump());
    spdlog::info("{}", gather_ctx->js.dump());

    spdlog::trace("Graph task done ...");
}

WFGraphTask* HDService::build_task_graph(WFHttpTask *server_task,
                                        const std::vector<std::string>& host_list) 
{
    spdlog::trace("Build task graph ...");
    auto *gather_ctx = static_cast<GatherCtx *>(server_task->user_data);

    WFGraphTask *graph = WFTaskFactory::create_graph_task(__graph_callback);
    graph->user_data = server_task;

    if(gather_ctx->ipv4)
    {
        WFGoTask *cache_v4_task = WFTaskFactory::create_go_task("batch", go_dns_cache, server_task, host_list, true);
        WFGraphNode& cache_v4_node = graph->create_graph_node(cache_v4_task);

        WFGoTask *pwork_v4_task = WFTaskFactory::create_go_task("batch", HDFactory::start_dns_paralell, server_task, true);
        WFGraphNode& pwork_v4_node = graph->create_graph_node(pwork_v4_task);
        cache_v4_node-->pwork_v4_node;

        spdlog::trace("Connect ipv4 node ...");
    }
    if(gather_ctx->ipv6)
    {
        WFGoTask *cache_v6_task = WFTaskFactory::create_go_task("batch", go_dns_cache, server_task, host_list, false);
        WFGraphNode& cache_v6_node = graph->create_graph_node(cache_v6_task);

        WFGoTask *pwork_v6_task = WFTaskFactory::create_go_task("batch", HDFactory::start_dns_paralell, server_task, false);
        WFGraphNode& pwork_v6_node = graph->create_graph_node(pwork_v6_task);
        cache_v6_node-->pwork_v6_node;

        spdlog::trace("Connect ipv4 node ...");
    }	
    return graph;
}

bool HDService::get_dns_cache_batch(WFHttpTask *server_task,
                                    const std::string &host,
                                    bool ipv4)
{
    DnsCtx *dns_ctx = new DnsCtx;
    dns_ctx->host = host;

    auto *dns_cache = WFGlobal::get_dns_cache();
    auto *addr_handle = dns_cache->get(host, 0);
    
    if(addr_handle) 
    {
        spdlog::info("Batch get cache");
        struct addrinfo *addrinfo = addr_handle->value.addrinfo;
        dns_ctx->ttl = addr_handle->value.expire_time;
        char ip_str[IP_STR_LEN];
        do {
            if(ipv4 && addrinfo->ai_family == AF_INET)
            {
                inet_ntop(AF_INET, &((reinterpret_cast<struct sockaddr_in *>(addrinfo->ai_addr))->sin_addr), ip_str, IP_STR_LEN);
                dns_ctx->ips.push_back(ip_str);
            }
            else if(!ipv4 && addrinfo->ai_family == AF_INET6)
            {
                inet_ntop(AF_INET6, &((reinterpret_cast<struct sockaddr_in6 *>(addrinfo->ai_addr))->sin6_addr), ip_str, IP_STR_LEN);
                dns_ctx->ips.push_back(ip_str);
            }
            addrinfo = addrinfo->ai_next;
        } while(addrinfo);

        dns_cache->release(addr_handle);

        auto *gather_ctx = static_cast<GatherCtx *>(server_task->user_data);
        gather_ctx->dns_ctx_gather_list.push_back(dns_ctx);
    }
    else 
    {
        spdlog::info("No cache found ...");
        delete dns_ctx;
        return false;
    }
    return true;
}


void HDService::go_dns_cache(WFHttpTask *server_task,
                                    const std::vector<std::string> &url_list,
                                    bool ipv4)
{
    spdlog::trace("Go dns cache");
    std::vector<std::string> not_in_dns_list;
    for(auto &url : url_list)
    {
        // not find dns record reserve to http req
        if(!get_dns_cache_batch(server_task, url, ipv4))
        {
            not_in_dns_list.push_back(url);
        }
    }
    auto *gather_ctx = static_cast<GatherCtx *>(server_task->user_data);
    if(ipv4) 
    {   
        gather_ctx->not_in_cache_v4 = std::move(not_in_dns_list);
    }
    else 
    {
        gather_ctx->not_in_cache_v6 = std::move(not_in_dns_list);
    }
}


void HDService::batch_dns_resolve(WFHttpTask *server_task,
                                  std::map<std::string, std::string> &query_split)
{
    spdlog::trace("Batch dns request ...");

    GatherCtx *gather_ctx = new GatherCtx;
    server_task->user_data = gather_ctx;
    check_query_field(gather_ctx, query_split);

    server_task->set_callback([](WFHttpTask *server_task)
    {
        // delete all the object on heap here
        auto *gather_ctx = static_cast<GatherCtx *>(server_task->user_data);
        for(auto* dns_ctx : gather_ctx->dns_ctx_gather_list)
        {
            delete dns_ctx;
        }
        delete gather_ctx;
        spdlog::trace("delete dns_ctx and gather ctx");
        spdlog::info("server task done");
    });
	auto host_list = StringUtil::split(query_split["host"], ',');
	if (host_list.empty())
	{
        server_task->get_resp()->append_output_body_nocopy(R"("code": "MissingArgument")", 25);
		spdlog::error("Host field is required ...");
		return;
	}

    auto *graph = build_task_graph(server_task, host_list);
    
    **server_task << graph;
}





