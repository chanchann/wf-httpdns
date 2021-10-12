#ifndef FACTORY_H
#define FACTORY_H

#include <workflow/Workflow.h>
#include <workflow/WFTaskFactory.h>
#include <workflow/HttpMessage.h>
#include <spdlog/spdlog.h>
#include <workflow/WFDnsClient.h>
#include <workflow/DnsUtil.h>
#include <workflow/StringUtil.h>
#include <mutex>
#include <vector>
#include "util.h"
#include "context.h"

using namespace protocol;

void dns_callback_multi(WFDnsTask *dns_task)
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
				dns_ctx->ips.emplace_back(ipBinToStr(record->rdata));
				dns_ctx->ttl = record->ttl;
			}
		}
		auto pwork = dns_task->get_parent_task();
		auto para_ctx = static_cast<parallel_context *>(series_of(pwork)->get_context());
		dns_ctx->client_ip = getPeerAddrStr(para_ctx->server_task);
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


void dns_callback(WFDnsTask *dns_task)
{
	if (dns_task->get_state() == WFT_STATE_SUCCESS)
	{
		spdlog::info("request DNS successfully...");

		single_dns_context *sin_ctx =
			static_cast<single_dns_context *>(series_of(dns_task)->get_context());
		auto dns_resp = dns_task->get_resp();
		DnsResultCursor cursor(dns_resp);
		dns_record *record = nullptr;
		std::vector<std::string> ips;
		while (cursor.next(&record))
		{
			if (record->type == DNS_TYPE_A)
			{	
				ips.emplace_back(ipBinToStr(record->rdata));
				sin_ctx->js["ttl"] = record->ttl;
			}
		}
		sin_ctx->js["ips"] = ips;
		HttpResponse *server_resp = sin_ctx->server_task->get_resp();
		server_resp->append_output_body(sin_ctx->js.dump());
	}
	else
	{
		spdlog::error("request DNS failed...");
	}
}

WFDnsTask *create_dns_task(WFDnsClient &dnsClient, const std::string &url, bool mutli = false)
{
	spdlog::trace("create dns task");
	WFDnsTask *dns_task;
	if(mutli)  
	{
		dns_task = dnsClient.create_dns_task(url, dns_callback_multi);
	}
	else 
	{
		dns_task = dnsClient.create_dns_task(url, dns_callback);
	}
	return dns_task;
}

SeriesWork *create_dns_series(WFDnsClient &dnsClient, const std::string &host) {
	spdlog::trace("create dns series");
	WFDnsTask *dns_task = create_dns_task(dnsClient, host, true);
	SeriesWork *series = Workflow::create_series_work(dns_task, nullptr);

	dns_context *dns_ctx = new dns_context;
	dns_ctx->host = host;
	series->set_context(dns_ctx);

	return series;
}

void parallel_callback(const ParallelWork *pwork)
{
	auto para_ctx = static_cast<parallel_context *>(series_of(pwork)->get_context());
	auto dns_cxt_list = para_ctx->dns_context_list;

	json dns_js;
	for(auto dns_ctx : dns_cxt_list)
	{
		to_json(dns_js, *dns_ctx);
		para_ctx->js["dns"].push_back(dns_js);
		delete dns_ctx;   
	}

	para_ctx->server_task->get_resp()->append_output_body(para_ctx->js.dump());

	spdlog::info("All series in this parallel have done");
}

ParallelWork *create_dns_paralell(WFDnsClient &dnsClient,
								  std::map<std::string, std::string> &query_split)
{
	auto host_list = StringUtil::split(query_split["host"], ',');
	if (host_list.empty())
	{
		spdlog::error("host is required field");
		return nullptr;
	}

	ParallelWork *pwork = Workflow::create_parallel_work(parallel_callback);
	for (auto &host : host_list)
	{
		auto series = create_dns_series(dnsClient, host);
		pwork->add_series(series);
	}
	return pwork;
}

#endif // FACTORY_H