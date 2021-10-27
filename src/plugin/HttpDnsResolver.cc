#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <utility>
#include <string>
#include <workflow/DnsRoutine.h>
#include <workflow/EndpointParams.h>
#include <workflow/RouteManager.h>
#include <workflow/WFGlobal.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/WFResourcePool.h>
#include <workflow/WFNameService.h>
#include <workflow/DnsCache.h>
#include <workflow/DnsUtil.h>
#include <workflow/WFDnsClient.h>
#include "HttpDnsResolver.h"

/*
DNS_CACHE_LEVEL_0	->	NO cache
DNS_CACHE_LEVEL_1	->	TTL MIN
DNS_CACHE_LEVEL_2	->	TTL [DEFAULT]
DNS_CACHE_LEVEL_3	->	Forever
*/

#define DNS_CACHE_LEVEL_0		0
#define DNS_CACHE_LEVEL_1		1
#define DNS_CACHE_LEVEL_2		2
#define DNS_CACHE_LEVEL_3		3


class HDResolverTask : public WFRouterTask
{
public:
	HDResolverTask(const struct WFNSParams *params, int dns_cache_level,
				   unsigned int dns_ttl_default, unsigned int dns_ttl_min,
				   const struct EndpointParams *endpoint_params,
				   router_callback_t&& cb) :
		WFRouterTask(std::move(cb))
	{
		type_ = params->type;
		host_ = params->uri.host ? params->uri.host : "";
		port_ = params->uri.port ? atoi(params->uri.port) : 0;
		info_ = params->info;
		dns_cache_level_ = dns_cache_level;
		dns_ttl_default_ = dns_ttl_default;
		dns_ttl_min_ = dns_ttl_min;
		endpoint_params_ = *endpoint_params;
		first_addr_only_ = params->fixed_addr;
	}

private:
	virtual void dispatch();
	virtual SubTask *done();
	void thread_dns_callback(ThreadDnsTask *dns_task);
	void dns_single_callback(WFDnsTask *dns_task);
	static void dns_partial_callback(WFDnsTask *dns_task);
	void dns_parallel_callback(const ParallelWork *pwork);
	void dns_callback_internal(DnsOutput *dns_task,
							   unsigned int ttl_default,
							   unsigned int ttl_min);

private:
	TransportType type_;
	std::string host_;
	std::string info_;
	unsigned short port_;
	bool first_addr_only_;
	bool query_dns_;
	int dns_cache_level_;
	unsigned int dns_ttl_default_;
	unsigned int dns_ttl_min_;
	struct EndpointParams endpoint_params_;
};

WFRouterTask *
HttpDnsResolver::create(const struct WFNSParams *params, int dns_cache_level,
					  unsigned int dns_ttl_default, unsigned int dns_ttl_min,
					  const struct EndpointParams *endpoint_params,
					  router_callback_t&& callback)
{
	return new HDResolverTask(params, dns_cache_level, dns_ttl_default,
							  dns_ttl_min, endpoint_params,
							  std::move(callback));
}

WFRouterTask *HttpDnsResolver::create_router_task(const struct WFNSParams *params,
												router_callback_t callback)
{
	const auto *settings = WFGlobal::get_global_settings();
	unsigned int dns_ttl_default = settings->dns_ttl_default;
	unsigned int dns_ttl_min = settings->dns_ttl_min;
	const struct EndpointParams *endpoint_params = &settings->endpoint_params;
	int dns_cache_level = params->retry_times == 0 ? DNS_CACHE_LEVEL_2 :
													 DNS_CACHE_LEVEL_1;
	return create(params, dns_cache_level, dns_ttl_default, dns_ttl_min,
				  endpoint_params, std::move(callback));
}

HttpDnsResolver::HttpDnsResolver() :
	respool(WFGlobal::get_global_settings()->dns_server_params.max_connections)
{
}

