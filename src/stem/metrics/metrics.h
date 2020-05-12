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

protected:
    template <typename ctype>
    struct CounterViewImpl;

public:
    class counter_u
    {
        friend struct CounterViewImpl<counter_u>;
        friend class metrics;
        typedef std::atomic<uint64_t> atomic_t;
        std::shared_ptr<atomic_t> _val;

        uint64_t flush()
        {
            if (_val)
                return _val->exchange(0, std::memory_order_consume);
            return 0;
        }

    public:
        counter_u() : _val(std::make_shared<atomic_t>()) { _val->store(0); }

        counter_u &operator++()
        {
            if (_val)
                _val->fetch_add(1, std::memory_order_consume);
            return *this;
        }
        counter_u &operator+=(uint64_t count)
        {
            if (_val)
                _val->fetch_add(count, std::memory_order_consume);
            return *this;
        }
    };

    class counter_d
    {
        friend struct CounterViewImpl<counter_d>;
        friend class metrics;

        typedef std::atomic<uint64_t> atomic_t;
        std::shared_ptr<atomic_t> _val;

        double flush()
        {
            if (!_val)
                return 0;
            double res = _val->exchange(0, std::memory_order_consume);
            return res / 10000.;
        }

    public:
        counter_d() : _val(std::make_shared<atomic_t>()) { _val->store(0); }

        counter_d &operator+=(double val)
        {
            if (!_val)
                return *this;

            if (!_val)
                return *this;

            uint64_t v = val * 10000.;
            if (_val)
                _val->fetch_add(v, std::memory_order_consume);

            return *this;
        }
    };

    class collector_u
    {
        friend struct CounterViewImpl<collector_u>;
        union ValueT {
            struct
            {
                uint64_t v : 44;
                uint64_t c : 20;
            };
            uint64_t r;
        };
        typedef uint32_t value_t;
        friend class metrics;
        typedef std::atomic<uint64_t> atomic_t;
        std::shared_ptr<atomic_t> _val;

        value_t flush()
        {
            if (!_val)
                return 0;

            ValueT v;
            v.r = _val->exchange(0, std::memory_order_consume);
            uint64_t res = v.c != 0 ? v.v / v.c : 0;

            if (res > 1000000)
                return 0;
            return res;
        }

    public:
        collector_u() : _val(std::make_shared<atomic_t>()) { _val->store(0); }

        collector_u &operator+=(value_t val)
        {
            if (!_val)
                return *this;

            ValueT v;
            v.v = val;
            v.c = 1;
            if (_val)
                _val->fetch_add(v.r, std::memory_order_consume);

            return *this;
        }
    };

protected:
    struct CounterView
    {
        virtual ~CounterView() {}
        virtual std::string flush() = 0;
        virtual bool connect(counter_u &) { return false; }
        virtual bool connect(counter_d &) { return false; }
        virtual bool connect(collector_u &) { return false; }
    };

    template <typename ctype>
    struct CounterViewImpl : public CounterView
    {
    public:
        ctype counter;

        CounterViewImpl(ctype _counter) : counter(_counter) {}

        virtual ~CounterViewImpl() {}
        virtual std::string flush() override
        {
            return std::to_string(counter.flush());
        }
        virtual bool connect(ctype &mtr) override
        {
            mtr._val = counter._val;
            return true;
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
    void connect(ctype &counter, std::string name, std::string root = "")
    {
        std::string mtr = getName(std::move(root), std::move(name));
        std::lock_guard<std::mutex> lock(m_mutex);
        auto itr = m_counterMap.find(mtr);
        if (m_counterMap.find(mtr) != m_counterMap.end())
        {
            if (!itr->second->connect(counter))
            {
                std::string mess = "Error at duplicate metrics '" + mtr + "': other type";
                throw std::runtime_error(mess);
            }
            spdlog::debug("Metrics duplicated: {}", mtr);
        }
        else
        {
            m_counterMap[mtr] = std::unique_ptr<CounterView>(new CounterViewImpl<ctype>(counter));
            spdlog::debug("Metrics registered: {}", mtr);
        }
    }
};

} // namespace stem
