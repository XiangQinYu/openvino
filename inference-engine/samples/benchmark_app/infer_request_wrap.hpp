// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <inference_engine.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "statistics_report.hpp"

typedef std::chrono::high_resolution_clock Time;
typedef std::chrono::nanoseconds ns;

typedef std::function<void(size_t id, const double latency)> QueueCallbackFunction;

/// @brief Wrapper class for InferenceEngine::InferRequest. Handles asynchronous callbacks and calculates execution
/// time.
class InferReqWrap final {
public:
    using Ptr = std::shared_ptr<InferReqWrap>;

    ~InferReqWrap() = default;

    explicit InferReqWrap(InferenceEngine::ExecutableNetwork& net, size_t id, QueueCallbackFunction callbackQueue)
        : _request(net.CreateInferRequest()),
          _id(id),
          _callbackQueue(callbackQueue) {
        _request.SetCompletionCallback([&]() {
            _endTime = Time::now();
            _callbackQueue(_id, getExecutionTimeInMilliseconds());
        });
    }

    void startAsync() {
        _startTime = Time::now();
        _request.StartAsync();
    }

    void wait() {
        _request.Wait(InferenceEngine::InferRequest::RESULT_READY);
    }

    void infer() {
        _startTime = Time::now();
        _request.Infer();
        _endTime = Time::now();
        _callbackQueue(_id, getExecutionTimeInMilliseconds());
    }

    std::map<std::string, InferenceEngine::InferenceEngineProfileInfo> getPerformanceCounts() {
        return _request.GetPerformanceCounts();
    }

    InferenceEngine::Blob::Ptr getBlob(const std::string& name) {
        return _request.GetBlob(name);
    }

    void setBlob(const std::string& name, const InferenceEngine::Blob::Ptr& data) {
        _request.SetBlob(name, data);
    }

    double getExecutionTimeInMilliseconds() const {
        auto execTime = std::chrono::duration_cast<ns>(_endTime - _startTime);
        return static_cast<double>(execTime.count()) * 0.000001;
    }

private:
    InferenceEngine::InferRequest _request;
    Time::time_point _startTime;
    Time::time_point _endTime;
    size_t _id;
    QueueCallbackFunction _callbackQueue;
};

class InferRequestsQueue final {
public:
    InferRequestsQueue(InferenceEngine::ExecutableNetwork& net, size_t nireq) {
        for (size_t id = 0; id < nireq; id++) {
            requests.push_back(std::make_shared<InferReqWrap>(
                net,
                id,
                std::bind(&InferRequestsQueue::putIdleRequest, this, std::placeholders::_1, std::placeholders::_2)));
            _idleIds.push(id);
        }
        resetTimes();
    }
    ~InferRequestsQueue() {
        // Inference Request guarantee that it will wait for all asynchronous internal tasks in destructor
        // So it should be released before any context that the request can use inside internal asynchronous tasks
        // For example all members of InferRequestsQueue would be destroyed before `requests` vector
        // So requests can try to use this members from `putIdleRequest()` that would be called from request callback
        // To avoid this we should move this vector declaration after all members declaration or just clear it manually
        // in destructor
        requests.clear();
    }

    void resetTimes() {
        _startTime = Time::time_point::max();
        _endTime = Time::time_point::min();
        _latencies.clear();
    }

    double getDurationInMilliseconds() {
        return std::chrono::duration_cast<ns>(_endTime - _startTime).count() * 0.000001;
    }

    void putIdleRequest(size_t id, const double latency) {
        std::unique_lock<std::mutex> lock(_mutex);
        _latencies.push_back(latency);
        _idleIds.push(id);
        _endTime = std::max(Time::now(), _endTime);
        _cv.notify_one();
    }

    InferReqWrap::Ptr getIdleRequest() {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this] {
            return _idleIds.size() > 0;
        });
        auto request = requests.at(_idleIds.front());
        _idleIds.pop();
        _startTime = std::min(Time::now(), _startTime);
        return request;
    }

    void waitAll() {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this] {
            return _idleIds.size() == requests.size();
        });
    }

    std::vector<double> getLatencies() {
        return _latencies;
    }

    std::vector<InferReqWrap::Ptr> requests;

private:
    std::queue<size_t> _idleIds;
    std::mutex _mutex;
    std::condition_variable _cv;
    Time::time_point _startTime;
    Time::time_point _endTime;
    std::vector<double> _latencies;
};
