#pragma once

#include <functional>

class IDispatcher {
public:
    virtual ~IDispatcher() = default;
    virtual void Post(std::function<void()> action) = 0;
};
