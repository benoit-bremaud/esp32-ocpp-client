#pragma once

#include "../dto/Results.h"

namespace Core::Application {

template<typename TRequest, typename TResponse>
class IUseCase {
public:
    virtual ~IUseCase() = default;
    virtual CommandResult<TResponse> execute(const TRequest& request) = 0;
};

} // namespace Core::Application
