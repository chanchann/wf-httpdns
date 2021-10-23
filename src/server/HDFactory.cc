#include "HDFactory.h"
#include "Util.h"
#include "Context.h"
#include <workflow/DnsRoutine.h>

static inline void __parallel_callback(const ParallelWork *pwork)
{
	auto para_ctx = static_cast<ParaDnsCtx *>(pwork->get_context());

	auto server_task = para_ctx->server_task;

	auto *dns_cache = WFGlobal::get_dns_cache();
    const auto *settings = WFGlobal::get_global_settings();
    unsigned int dns_ttl_default = settings->dns_ttl_default;
    unsigned int dns_ttl_min = settings->dns_ttl_min;
    const DnsCache::DnsHandle *addr_handle;

	auto gather_ctx = static_cast<GatherCtx *>(server_task->user_data);
	{
		for (auto *dns_ctx :  para_ctx->DnsCtx_list)
		{
			// put to cache
			addr_handle = dns_cache->put(dns_ctx->host, 
										dns_ctx->port, 
										dns_ctx->addrinfo,
										dns_ttl_default,
										dns_ttl_min);
			dns_cache->release(addr_handle);
			// gather 
			std::lock_guard<std::mutex> lock(gather_ctx->mutex);
			gather_ctx->dns_ctx_gather_list.push_back(dns_ctx);
		}
	}
	delete para_ctx;
	spdlog::trace("parallel context delete");
	spdlog::info("All series in this parallel have done");
}

SeriesWork *HDFactory::create_dns_series(ParallelWork *pwork, const std::string &host)
{
	spdlog::trace("create dns series");
	WFDnsTask *dns_task = create_dns_task(host, true);

	SeriesWork *series = Workflow::create_series_work(dns_task, [](const SeriesWork*){
		spdlog::info("tasks in series have done");
	});

	DnsCtx *dns_ctx = new DnsCtx;
	dns_ctx->host = host;
	dns_ctx->para_ctx = static_cast<ParaDnsCtx *>(pwork->get_context());
	dns_task->user_data = dns_ctx;

	return series;
}

ParallelWork *HDFactory::create_dns_paralell(WFHttpTask *server_task, bool ipv4)
{
	auto *gather_ctx = static_cast<GatherCtx *>(server_task->user_data);
	auto *go_ctx = gather_ctx->go_ctx;
	
	std::vector<std::string> not_int_cache_list;
	if(ipv4)
	{
		not_int_cache_list = go_ctx->not_int_cache_map["ipv4"];
	}
	else 
	{
		not_int_cache_list = go_ctx->not_int_cache_map["ipv6"];
	}
	if(not_int_cache_list.empty()) return nullptr;
	
	ParallelWork *pwork = Workflow::create_parallel_work(__parallel_callback);
	ParaDnsCtx *para_ctx = new ParaDnsCtx;
	para_ctx->server_task = server_task;
	pwork->set_context(para_ctx);

	for (auto &host : not_int_cache_list)
	{
		pwork->add_series(create_dns_series(pwork, host));
	}

	return pwork;
}

static inline void __dns_callback(WFDnsTask *dns_task)
{
	if (dns_task->get_state() == WFT_STATE_SUCCESS)
	{
		spdlog::info("request DNS successfully...");

		auto *sin_ctx =
			static_cast<SingleDnsCtx *>(dns_task->user_data);
		
		auto dns_resp = dns_task->get_resp();
		DnsResultCursor cursor(dns_resp);
		dns_record *record = nullptr;
		while (cursor.next(&record))
		{
			if (record->type == DNS_TYPE_A)
			{
				sin_ctx->ips.emplace_back(Util::ip_bin_to_str(record->rdata));
			}
			else if (record->type == DNS_TYPE_AAAA)
			{
				sin_ctx->ipsv6.emplace_back(Util::ip_bin_to_str(record->rdata, false));
			}
			sin_ctx->ttl = record->ttl;
		}

		// put to cache
		struct addrinfo *ai = NULL;
		
		int ret = DnsUtil::getaddrinfo(dns_task->get_resp(), sin_ctx->port, &ai);
		DnsOutput out;
		DnsRoutine::create(&out, ret, ai);		
		struct addrinfo *addrinfo = out.move_addrinfo();   // key

		sin_ctx->addrinfo = addrinfo;
	}
	else
	{	
		spdlog::error("request DNS failed, state = {}, error = {}, timeout reason = {}...", 
				dns_task->get_state(), dns_task->get_error(), dns_task->get_timeout_reason());
	}
}

static inline void __dns_callback_batch(WFDnsTask *dns_task)
{
	
	if (dns_task->get_state() == WFT_STATE_SUCCESS)
	{
		spdlog::info("request DNS successfully...");
		DnsCtx *dns_ctx =
			static_cast<DnsCtx *>(dns_task->user_data);

		auto dns_resp = dns_task->get_resp();
		DnsResultCursor cursor(dns_resp);
		dns_record *record = nullptr;
		while (cursor.next(&record))
		{
			if (record->type == DNS_TYPE_A)
			{
				dns_ctx->ips.emplace_back(Util::ip_bin_to_str(record->rdata));
			}
			else if (record->type == DNS_TYPE_AAAA)
			{
				dns_ctx->ips.emplace_back(Util::ip_bin_to_str(record->rdata, false));
			}
			dns_ctx->ttl = record->ttl;
		}
		auto para_ctx = dns_ctx->para_ctx;
		dns_ctx->client_ip = Util::get_peer_addr_str(para_ctx->server_task);
		{
			std::lock_guard<std::mutex> lock(para_ctx->mutex);
			para_ctx->DnsCtx_list.push_back(dns_ctx);
			// todo : we can jump this step, directly gather dns_ctx 
			// todo : and we put cache here .....
		}

		// gather addrinfo
		struct addrinfo *ai = NULL;
		
		int ret = DnsUtil::getaddrinfo(dns_resp, dns_ctx->port, &ai);
		DnsOutput out;
		DnsRoutine::create(&out, ret, ai);		
		struct addrinfo *addrinfo = out.move_addrinfo();   // key
		
		dns_ctx->addrinfo = addrinfo;     // todo : should we free ourselves?
	}
	else
	{
		spdlog::error("request DNS failed...");
	}
}

WFDnsTask *HDFactory::create_dns_task(const std::string &url, bool is_batch)
{
	spdlog::trace("create dns task");
	auto *dns_client = WFGlobal::get_dns_client();
	WFDnsTask *dns_task;
	if (is_batch)
	{
		dns_task = dns_client->create_dns_task(url, __dns_callback_batch);
	}
	else
	{
		dns_task = dns_client->create_dns_task(url, __dns_callback);
	}
	return dns_task;
}
