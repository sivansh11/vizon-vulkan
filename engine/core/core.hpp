#ifndef CORE_CORE_HPP
#define CORE_CORE_HPP

#include <memory>

namespace core {

template <typename T>
using ref = std::shared_ptr<T>;

template <typename T, typename... Args>
static ref<T> make_ref(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

} // namespace core

#endif