#ifndef _HTTPDNSRESOLVER_H_
#define _HTTPDNSRESOLVER_H_

#include <workflow/EndpointParams.h>
#include <workflow/WFNameService.h>
#include <workflow/WFResourcePool.h>


class HttpDnsResolver : public WFNSPolicy
{
public:
	virtual WFRouterTask *create_router_task(const struct WFNSParams *params,
											 router_callback_t callback);

public:
	WFRouterTask *create(const struct WFNSParams *params, int dns_cache_level,
						 unsigned int dns_ttl_default, unsigned int dns_ttl_min,
						 const struct EndpointParams *endpoint_params,
						 router_callback_t&& callback);

private:
	WFResourcePool respool;

public:
	HttpDnsResolver();
	// friend class WFResolverTask;
};


#endif // _HTTPDNSRESOLVER_H_