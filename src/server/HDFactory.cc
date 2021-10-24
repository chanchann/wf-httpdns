#include "HDFactory.h"
#include "Util.h"
#include "Context.h"
#include <workflow/DnsRoutine.h>

SeriesWork *HDFactory::create_dns_series(ParallelWork *pwork, const std::string &host)
{
	spdlog::trace("create dns series");
	DnsCtx *dns_ctx = new DnsCtx;
	if(!Util::split_host_port(dns_ctx, host))
	{
		delete dns_ctx;
		spdlog::error("url is invalid");
		return nullptr;
	}
	dns_ctx->server_task = static_cast<WFHttpTask *>(pwork->get_context());

	WFDnsTask *dns_task = create_dns_task(host, true);
	dns_task->user_data = dns_ctx;
	SeriesWork *series = Workflow::create_series_work(dns_task, [](const SeriesWork*){
		spdlog::info("tasks in series have done");
	});
	return series;
}

void HDFactory::start_dns_paralell(WFHttpTask *server_task, bool ipv4)
{	
	std::vector<std::string> *not_in_cache_list;
	auto *gather_ctx = static_cast<GatherCtx *>(server_task->user_data);
	if(ipv4)
	{
		not_in_cache_list = &gather_ctx->not_in_cache_v4;
	} else 
	{
		not_in_cache_list = &gather_ctx->not_in_cache_v6;
	}
	if((*not_in_cache_list).empty()) return;
	
	ParallelWork *pwork = Workflow::create_parallel_work([](const ParallelWork *pwork){
		spdlog::trace("All series in this parallel have done");
	});

	pwork->set_context(server_task);
	
	for (auto &host : *not_in_cache_list)
	{
		spdlog::info("host : {}", host);
		pwork->add_series(create_dns_series(pwork, host));
	}
	pwork->start();
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
		auto server_task = dns_ctx->server_task;
		auto gather_ctx = static_cast<GatherCtx *>(server_task->user_data);
		dns_ctx->client_ip = Util::get_peer_addr_str(server_task);

		// put cache
		struct addrinfo *ai = NULL;
		int ret = DnsUtil::getaddrinfo(dns_resp, dns_ctx->port, &ai);
		DnsOutput out;
		DnsRoutine::create(&out, ret, ai);		
		struct addrinfo *addrinfo = out.move_addrinfo();   // key

		auto *dns_cache = WFGlobal::get_dns_cache();
		const auto *settings = WFGlobal::get_global_settings();
		unsigned int dns_ttl_default = settings->dns_ttl_default;
		unsigned int dns_ttl_min = settings->dns_ttl_min;

		spdlog::info("put cache : {} - {}", dns_ctx->host, dns_ctx->port);
		auto *addr_handle = dns_cache->put(dns_ctx->host, 
										dns_ctx->port, 
										addrinfo,
										dns_ttl_default,
										dns_ttl_min);
		dns_cache->release(addr_handle);

		{
			std::lock_guard<std::mutex> lock(gather_ctx->mutex);
			gather_ctx->dns_ctx_gather_list.push_back(dns_ctx);
		}
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
