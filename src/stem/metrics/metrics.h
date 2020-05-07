#pragma once

#include <string>
#include <sstream>
#include <chrono>
#include <atomic>
#include <mutex>

#include <map>

#include <spdlog/spdlog.h>

#include <concurrentqueue/blockingconcurrentqueue.h>

#include <memory>

#include "stem/tcp_socket.h"

namespace stem
{

class metrics
{
public:
    template <typename value_t>
    class counter_t
    {
        friend class metrics;
        typedef std::atomic<value_t> atomic_t;
        std::shared_ptr<atomic_t> _val;

    public:
        counter_t() : _val(std::make_shared<atomic_t>(0)) {}
        void reset() { _val.reset(); }

        counter_t &operator++()
        {
            if (_val)
                _val->fetch_add(1, std::memory_order_relaxed);
            return *this;
        }
        counter_t &operator+=(value_t count)
        {
            if (_val)
                _val->fetch_add(count, std::memory_order_relaxed);
            return *this;
        }
        value_t flush()
        {
            if (_val)
                return _val->exchange(0, std::memory_order_relaxed);
            return 0;
        }
    };
    typedef counter_t<unsigned long long> counter_u;

    class counter_d
    {
        typedef double value_t;
        friend class metrics;
        typedef std::atomic<value_t> atomic_t;
        std::shared_ptr<atomic_t> _val;

    public:
        counter_d() : _val(std::make_shared<atomic_t>(0)) {}
        void reset() { _val.reset(); }

        counter_d &operator+=(value_t val)
        {
            if (!_val)
                return *this;

            value_t oldV;
            do
            {
                oldV = _val->load(std::memory_order_relaxed);
            } while (!_val->compare_exchange_weak(oldV, oldV + val), std::memory_order_relaxed);

            return *this;
        }
        value_t flush()
        {
            if (_val)
                return _val->exchange(0, std::memory_order_relaxed);
            return 0;
        }
    };

    template <typename value_t>
    class collector_t
    {
        struct value
        {
            value_t v;
            uint32_t c;
        };
        friend class metrics;
        typedef std::atomic<value> atomic_t;
        std::shared_ptr<atomic_t> val;

    public:
        collector_t() : val(std::make_shared<atomic_t>(0)) {}

        void reset() { val.reset(); }

        collector_t &operator+=(value_t _val)
        {
            if (!val)
                return *this;

            value oldV;
            do
            {
                oldV = val->load(std::memory_order_relaxed);
            } while (!val->compare_exchange_weak(oldV, {oldV.v + _val, oldV.c + 1}, std::memory_order_relaxed));

            return *this;
        }
        value_t flush()
        {
            if (!val)
                return 0;

            value _val = val->exchange({0, 0}, std::memory_order_relaxed);
            return _val.c != 0 ? _val.v / _val.c : 0;
        }
    };
    typedef collector_t<unsigned long long> collector_u;
    typedef collector_t<double> collector_d;

protected:
    struct CounterView
    {
        virtual ~CounterView() {}
        virtual std::string flush() = 0;
    };

    template <typename ctype>
    struct CounterViewImpl : public CounterView
    {
        ctype counter;

        CounterViewImpl(ctype _counter) : counter(_counter) {}

        virtual ~CounterViewImpl() {}
        virtual std::string flush() override
        {
            return std::to_string(counter.flush());
        }
    };

    typedef moodycamel::BlockingConcurrentQueue<std::string> SendQueue;

    std::atomic_bool m_active;

    SocketOutput socket_output_;
    SocketHolder socket_;

    std::string m_host;
    uint16_t m_port;
    std::string m_root;

    std::atomic<bool> m_run;
    std::thread m_sendThread;
    std::thread m_agregateThread;

    std::unique_ptr<SendQueue> m_sendQueue;
    std::map<std::string, uint64_t> m_metricsMap;

    std::mutex m_mutex;
    std::map<std::string, std::unique_ptr<CounterView>> m_counterMap;

    std::shared_ptr<spdlog::logger> m_logger;

    void ResetConnection();

    void sendProc();
    void agregateProc();

    std::string getName(std::string root, std::string name);

public:
    metrics(std::string host, uint16_t port, std::string root, std::shared_ptr<spdlog::logger> logger);
    ~metrics();

    void stop();
    void start();

    template <typename ctype>
    void connect(ctype &counter, std::string name,std::string root="")
    {
        std::string mtr = getName(std::move(root), std::move(name));
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_counterMap.find(mtr) != m_counterMap.end())
        {
            throw "Duplicate metrics";
        }
        spdlog::debug("Metrics registered: {}",mtr);

        m_counterMap[mtr] = std::unique_ptr<CounterView>(new CounterViewImpl<ctype>(counter));
    }
};

} // namespace