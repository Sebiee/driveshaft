/*
 * Driveshaft: Gearman worker manager
 *
 * Copyright (c) [2015] [Keyur Govande]
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "thread-registry.h"

namespace Driveshaft {

#define DS_ASSERT(cond, logger)\
    do {\
        if (!(cond)) {\
            LOG4CXX_ERROR(logger, "Failed assertion: " << #cond);\
            throw std::runtime_error(#cond);\
        }\
    } while(0)

ThreadRegistry::ThreadRegistry() noexcept : m_thread_map(), m_registry_store(), m_mutex() {
    LOG4CXX_DEBUG(MainLogger, "Starting thread registry");
}

ThreadRegistry::~ThreadRegistry() noexcept {
    DS_ASSERT(m_thread_map.size() == 0, MainLogger);
}

void ThreadRegistry::registerThread(const std::string& pool, std::thread::id tid) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG4CXX_DEBUG(ThreadLogger, "Registering thread for pool " << pool << " with ID " << tid);

    auto& pool_threads = m_registry_store[pool];
    DS_ASSERT(pool_threads.count(tid) == 0, ThreadLogger);
    pool_threads.insert(tid);

    DS_ASSERT(m_thread_map.count(tid) == 0, ThreadLogger);
    m_thread_map.insert(std::pair<std::thread::id, ThreadData>(tid, {pool, false, "Starting up"}));
}

void ThreadRegistry::unregisterThread(const std::string& pool, std::thread::id tid) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG4CXX_DEBUG(ThreadLogger, "Unregistering thread for pool " << pool << " with ID " << tid);

    auto& pool_threads = m_registry_store[pool];
    DS_ASSERT(pool_threads.count(tid) == 1, ThreadLogger);
    pool_threads.erase(tid);

    DS_ASSERT(m_thread_map.count(tid) == 1, ThreadLogger);
    m_thread_map.erase(tid);
}

uint32_t ThreadRegistry::poolCount(const std::string& pool) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG4CXX_DEBUG(MainLogger, "Getting poolCount for pool " << pool);

    return m_registry_store[pool].size();
}

bool ThreadRegistry::sendShutdown(const std::string& pool, uint32_t count) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG4CXX_INFO(MainLogger, "Sending shutdown to pool " << pool << ". Count " << count);

    const auto& pool_threads = m_registry_store[pool];
    uint32_t msg_sent = 0;

    for (auto tid : pool_threads) {
        DS_ASSERT(m_thread_map.count(tid) == 1, MainLogger);
        auto& threaddata = m_thread_map[tid];
        if (threaddata.should_shutdown == false) {
            threaddata.should_shutdown = true;
            ++msg_sent;
            if (msg_sent == count) {
                break;
            }
        }
    }

    LOG4CXX_INFO(MainLogger, "Shutdown sent to " << msg_sent << " members of pool " << pool);

    return msg_sent == count;
}

bool ThreadRegistry::shouldShutdown(std::thread::id tid) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG4CXX_DEBUG(ThreadLogger, "Checking shouldShutdown for ID " << tid);

    DS_ASSERT(m_thread_map.count(tid) == 1, ThreadLogger);
    return m_thread_map[tid].should_shutdown;
}

void ThreadRegistry::setThreadState(std::thread::id tid, const std::string& state) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG4CXX_DEBUG(ThreadLogger, "Checking shouldShutdown for ID " << tid);

    DS_ASSERT(m_thread_map.count(tid) == 1, ThreadLogger);
    m_thread_map[tid].state = state;
}

ThreadMap ThreadRegistry::getThreadMap() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);

    return m_thread_map;
}
}
