#ifndef _HDSERVICE_H_
#define _HDSERVICE_H_

#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/HttpMessage.h>
#include <spdlog/spdlog.h>
#include <workflow/WFDnsClient.h>
#include <workflow/DnsUtil.h>
#include <workflow/StringUtil.h>
#include <vector>


using namespace protocol;

class SingleDnsCtx;

class HDService
{
public:
	static void single_dns_resolve(WFHttpTask *server_task,
								   std::map<std::string, std::string> &query_split);

	static void batch_dns_resolve(WFHttpTask *server_task,
								  std::map<std::string, std::string> &query_split);

private:
	static bool get_dns_cache(WFHttpTask *server_task,
                            const std::string &host);

	static void go_dns_cache(WFHttpTask *server_task,
                                    const std::vector<std::string> &host_list,
                                    bool ipv4 = true);
    
    static bool get_dns_cache_batch(WFHttpTask *server_task,
                                    const std::string &host,
                                    bool ipv4 = true);

	static WFGraphTask *build_task_graph(WFHttpTask *server_task,
										const std::vector<std::string>& host_list);

	static std::string check_host_field(WFHttpTask *server_task,
										std::map<std::string, std::string> &query_split);
	template<typename T>
	static void check_query_field(T *ctx,
								  std::map<std::string, std::string> &query_split);
 
};

template<typename T>
void HDService::check_query_field(T *ctx,
								  std::map<std::string, std::string> &query_split) 
{
    if(query_split.find("query") != query_split.end())
    {   
        auto query_ipvx_list = StringUtil::split(query_split["query"], ',');
        for(auto& ipvx : query_ipvx_list) 
        {
            if(ipvx == "4") 
            {
                ctx->ipv4 = true;
            }
            if(ipvx == "6")
            {
                ctx->ipv6 = true;
            }
        }
    }
    else 
    {
        ctx->ipv4 = true;  // default
    }
}


#endif // _HDSERVICE_H_