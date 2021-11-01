#include "HDFactory.h"
#include "HttpUtil.h"
#include "Context.h"
#include <workflow/DnsRoutine.h>

static const int INET_ADDR_LEN = 128;

SeriesWork *HDFactory::create_dns_series(ParallelWork *pwork, const std::string &host)
{
    spdlog::trace("create dns series");
    auto *dns_ctx = new DnsCtx;
    dns_ctx->host = host;
    dns_ctx->server_task = static_cast<WFHttpTask *>(pwork->get_context());
    dns_ctx->origin_ttl = WFGlobal::get_global_settings()->dns_ttl_default;
    dns_ctx->ttl = dns_ctx->origin_ttl;
    dns_ctx->client_ip = HttpUtil::get_peer_addr_str(dns_ctx->server_task);

    WFDnsTask *dns_task = create_dns_task(host, true);
    dns_task->user_data = dns_ctx;
    SeriesWork *series = Workflow::create_series_work(dns_task, [](const SeriesWork *)
    {
        spdlog::info("tasks in series have done");
    });
    return series;
}

void HDFactory::start_dns_paralell(WFHttpTask *server_task, bool ipv4)
{
    std::vector<std::string> *not_in_cache_list;
    auto *gather_ctx = static_cast<GatherCtx *>(server_task->user_data);
    if (ipv4)
    {
        not_in_cache_list = &gather_ctx->not_in_cache_v4;
    } else
    {
        not_in_cache_list = &gather_ctx->not_in_cache_v6;
    }
    if ((*not_in_cache_list).empty()) return;

    ParallelWork *pwork = Workflow::create_parallel_work([](const ParallelWork *pwork)
                                                         {
                                                             spdlog::trace("All series in this parallel have done");
                                                         });

    pwork->set_context(server_task);

    for (auto &host: *not_in_cache_list)
    {
        spdlog::info("host : {}", host);
        pwork->add_series(create_dns_series(pwork, host));
    }
    pwork->start();
}

std::string __ip_bin_to_str(void *sa_sin_addr, bool ipv4 = true)
{
    struct sockaddr_in sa{};
    char str[INET_ADDR_LEN];
    if (ipv4)
        inet_ntop(AF_INET, sa_sin_addr, str, INET_ADDR_LEN);
    else
        inet_ntop(AF_INET6, sa_sin_addr, str, INET_ADDR_LEN);
    return str;
}

static inline void __put_cache(WFDnsTask *dns_task, const std::string &host)
{
    // put to cache
    struct addrinfo *ai = nullptr;
    int ret = DnsUtil::getaddrinfo(dns_task->get_resp(), 0, &ai);
    DnsOutput out;
    DnsRoutine::create(&out, ret, ai);
    struct addrinfo *addrinfo = out.move_addrinfo();   // key

    auto *dns_cache = WFGlobal::get_dns_cache();
    const auto *settings = WFGlobal::get_global_settings();
    unsigned int dns_ttl_default = settings->dns_ttl_default;
    unsigned int dns_ttl_min = settings->dns_ttl_min;
    const DnsCache::DnsHandle *addr_handle;
    addr_handle = dns_cache->put(host,
                                 0,
                                 addrinfo,
                                 dns_ttl_default,
                                 dns_ttl_min);
    dns_cache->release(addr_handle);
}

static inline void __dns_callback(WFDnsTask *dns_task)
{
    if (dns_task->get_state() == WFT_STATE_SUCCESS)
    {
        spdlog::info("Request DNS successfully...");

        auto *sin_ctx =
                static_cast<SingleDnsCtx *>(dns_task->user_data);

        auto dns_resp = dns_task->get_resp();
        DnsResultCursor cursor(dns_resp);
        dns_record *record = nullptr;

        while (cursor.next(&record))
        {
            if (record->type == DNS_TYPE_A)
            {
                sin_ctx->ips.emplace_back(__ip_bin_to_str(record->rdata));
            } else if (record->type == DNS_TYPE_AAAA)
            {
                sin_ctx->ipsv6.emplace_back(__ip_bin_to_str(record->rdata, false));
            }
        }

        __put_cache(dns_task, sin_ctx->host);
    } else
    {
        spdlog::error("request DNS failed, state = {}, error = {}, timeout reason = {}...",
                      dns_task->get_state(), dns_task->get_error(), dns_task->get_timeout_reason());
    }
}

static inline void __dns_callback_batch(WFDnsTask *dns_task)
{
    if (dns_task->get_state() == WFT_STATE_SUCCESS)
    {
        spdlog::info("Request DNS successfully...");
        auto *dns_ctx =
                static_cast<DnsCtx *>(dns_task->user_data);

        auto dns_resp = dns_task->get_resp();
        DnsResultCursor cursor(dns_resp);
        dns_record *record = nullptr;

        while (cursor.next(&record))
        {
            if (record->type == DNS_TYPE_A)
            {
                dns_ctx->ips.emplace_back(__ip_bin_to_str(record->rdata));
            } else if (record->type == DNS_TYPE_AAAA)
            {
                dns_ctx->ips.emplace_back(__ip_bin_to_str(record->rdata, false));
            }
        }
        auto server_task = dns_ctx->server_task;
        auto gather_ctx = static_cast<GatherCtx *>(server_task->user_data);

        __put_cache(dns_task, dns_ctx->host);

        std::lock_guard<std::mutex> lock(gather_ctx->mutex);
        gather_ctx->dns_ctx_gather_list.push_back(dns_ctx);
    } else
    {
        spdlog::error("Request DNS failed...");
    }
}

WFDnsTask *HDFactory::create_dns_task(const std::string &host, bool is_batch)
{
    spdlog::trace("Create dns task");
    auto *dns_client = WFGlobal::get_dns_client();
    WFDnsTask *dns_task;
    if (is_batch)
    {
        dns_task = dns_client->create_dns_task(host, __dns_callback_batch);
    } else
    {
        dns_task = dns_client->create_dns_task(host, __dns_callback);
    }
    return dns_task;
}
