#include "HDFactory.h"
#include "Util.h"
#include "Context.h"

static inline void __parallel_callback(const ParallelWork *pwork)
{
	auto para_ctx = static_cast<parallel_context *>(series_of(pwork)->get_context());
	auto dns_cxt_list = para_ctx->dns_context_list;

	json dns_js;
	for (auto dns_ctx : dns_cxt_list)
	{
		to_json(dns_js, *dns_ctx);
		para_ctx->js["dns"].push_back(dns_js);
		delete dns_ctx;
	}

	para_ctx->server_task->get_resp()->append_output_body(para_ctx->js.dump());

	spdlog::info("All series in this parallel have done");
}

SeriesWork *HDFactory::create_dns_series(WFDnsClient &dnsClient, const std::string &host)
{
	spdlog::trace("create dns series");
	WFDnsTask *dns_task = create_dns_task(dnsClient, host, true);
	SeriesWork *series = Workflow::create_series_work(dns_task, nullptr);

	dns_context *dns_ctx = new dns_context;
	dns_ctx->host = host;
	series->set_context(dns_ctx);

	return series;
}

ParallelWork *HDFactory::create_dns_paralell(WFDnsClient &dnsClient,
											 std::map<std::string, std::string> &query_split)
{
	auto host_list = StringUtil::split(query_split["host"], ',');
	if (host_list.empty())
	{
		spdlog::error("host is required field");
		return nullptr;
	}

	ParallelWork *pwork = Workflow::create_parallel_work(__parallel_callback);
	for (auto &host : host_list)
	{
		auto dns_series = create_dns_series(dnsClient, host);
		pwork->add_series(dns_series);
	}
	return pwork;
}

static inline void __dns_callback(WFDnsTask *dns_task)
{
	spdlog::debug("__dns_callback");
	if (dns_task->get_state() == WFT_STATE_SUCCESS)
	{
		spdlog::info("request DNS successfully...");

		auto sin_ctx =
			static_cast<single_dns_context *>(series_of(dns_task->get_parent_task())->get_context());
		spdlog::info("sin_ctx : {}", sin_ctx->test);
		
		auto dns_resp = dns_task->get_resp();
		DnsResultCursor cursor(dns_resp);
		dns_record *record = nullptr;
		std::vector<std::string> ips;
		int cnt = 0;
		bool ipv4 = true;
		while (cursor.next(&record))
		{
			if(cnt == 0)
			{
				// choose the first ttl
				sin_ctx->js["ttl"] = record->ttl;
				cnt++;
			}
			if (record->type == DNS_TYPE_A)
			{
				ips.emplace_back(Util::ip_bin_to_str(record->rdata));
			}
			else if (record->type == DNS_TYPE_AAAA)
			{
				ipv4 = false;
				ips.emplace_back(Util::ip_bin_to_str(record->rdata, ipv4));
			}
		}
		if(ipv4)
			sin_ctx->js["ips"] = ips;
		else 
			sin_ctx->js["ipsv6"] = ips;
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

		dns_context *dns_ctx =
			static_cast<dns_context *>(series_of(dns_task)->get_context());
		auto dns_resp = dns_task->get_resp();
		DnsResultCursor cursor(dns_resp);
		dns_record *record = nullptr;
		std::vector<std::string> ips;
		while (cursor.next(&record))
		{
			if (record->type == DNS_TYPE_A)
			{
				dns_ctx->ips.emplace_back(Util::ip_bin_to_str(record->rdata));
				dns_ctx->ttl = record->ttl;
			}
		}
		auto pwork = dns_task->get_parent_task();
		auto para_ctx = static_cast<parallel_context *>(series_of(pwork)->get_context());
		dns_ctx->client_ip = Util::get_peer_addr_str(para_ctx->server_task);
		{
			std::lock_guard<std::mutex> lock(para_ctx->mutex);
			para_ctx->dns_context_list.push_back(dns_ctx);
		}
	}
	else
	{
		spdlog::error("request DNS failed...");
	}
}

WFDnsTask *HDFactory::create_dns_task(WFDnsClient &dnsClient, const std::string &url, bool isMutli)
{
	spdlog::trace("create dns task");
	WFDnsTask *dns_task;
	if (isMutli)
	{
		dns_task = dnsClient.create_dns_task(url, __dns_callback_multi);
	}
	else
	{
		dns_task = dnsClient.create_dns_task(url, __dns_callback);
	}
	return dns_task;
}
