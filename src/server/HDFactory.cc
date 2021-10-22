#include "HDFactory.h"
#include "Util.h"
#include "Context.h"
#include <workflow/DnsRoutine.h>

static inline void __parallel_callback(const ParallelWork *pwork)
{
	auto para_ctx = static_cast<ParaDnsCtx *>(pwork->get_context());
	auto& dns_cxt_list = para_ctx->DnsCtx_list;

	auto server_task = para_ctx->server_task;
	auto gather_ctx = static_cast<GatherCtx *>(server_task->user_data);

	{
		std::lock_guard<std::mutex> lock(gather_ctx->mutex);
		for (auto *dns_ctx : dns_cxt_list)
		{
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
	series->set_context(dns_ctx);

	return series;
}

ParallelWork *HDFactory::create_dns_paralell(WFHttpTask *server_task,
											std::map<std::string, std::string> &query_split,
											bool ipv4)
{
	auto host_list = StringUtil::split(query_split["host"], ',');
	if (host_list.empty())
	{
		spdlog::error("host is required field");
		return nullptr;
	}

	ParallelWork *pwork = Workflow::create_parallel_work(__parallel_callback);
	ParaDnsCtx *para_ctx = new ParaDnsCtx;
	para_ctx->server_task = server_task;
	if(!ipv4)
		para_ctx->ipv4 = false;
	pwork->set_context(para_ctx);

	for (auto &host : host_list)
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
		std::vector<std::string> ips;
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
		const DnsCache::DnsHandle *addr_handle;
		auto *dns_cache = WFGlobal::get_dns_cache();

		// port fixed tempe
		const auto *settings = WFGlobal::get_global_settings();
		unsigned int dns_ttl_default = settings->dns_ttl_default;
		unsigned int dns_ttl_min = settings->dns_ttl_min;
		addr_handle = dns_cache->put(sin_ctx->host, sin_ctx->port, ai,
									 dns_ttl_default,
									 dns_ttl_min);

		dns_cache->release(addr_handle);
	}
	else
	{	
		spdlog::error("request DNS failed, state = {}, error = {}, timeout reason = {}...", 
				dns_task->get_state(), dns_task->get_error(), dns_task->get_timeout_reason());
	}
}

static inline void __dns_callback_multi(WFDnsTask *dns_task)
{
	if (dns_task->get_state() == WFT_STATE_SUCCESS)
	{
		spdlog::info("request DNS successfully...");
		
		DnsCtx *dns_ctx =
			static_cast<DnsCtx *>(series_of(dns_task)->get_context());
		auto dns_resp = dns_task->get_resp();
		DnsResultCursor cursor(dns_resp);
		dns_record *record = nullptr;
		std::vector<std::string> ips;

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
		}
	}
	else
	{
		spdlog::error("request DNS failed...");
	}
}

WFDnsTask *HDFactory::create_dns_task(const std::string &url, bool isMutli)
{
	spdlog::trace("create dns task");
	auto *dns_client = WFGlobal::get_dns_client();
	WFDnsTask *dns_task;
	if (isMutli)
	{
		dns_task = dns_client->create_dns_task(url, __dns_callback_multi);
	}
	else
	{
		dns_task = dns_client->create_dns_task(url, __dns_callback);
	}
	return dns_task;
}
