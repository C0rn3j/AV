#include "shared/structs.hpp"

#include <cassert>
#include <functional>

namespace globals {
    namespace detail {
        [[nodiscard]] inline std::uintptr_t base() {
            static auto base_addr = GetModuleHandleA("wsc");
            assert(base_addr != nullptr);
            return reinterpret_cast<std::uintptr_t>(base_addr);
        }

        template <typename RetTy>
        [[nodiscard]] RetTy initializer(const std::ptrdiff_t offset_x64, const std::ptrdiff_t offset_x86) {
            const auto base_addr = base();
            const auto offset = std::numeric_limits<size_t>::digits == 64 ? offset_x64 : offset_x86;

            if constexpr (std::is_same_v<std::uintptr_t, RetTy>) {
                return base_addr + offset;
            } else {
                return reinterpret_cast<RetTy>(base_addr + offset);
            }
        }
    } // namespace detail

    inline struct {
        HANDLE* queue_handle = detail::initializer<HANDLE*>(0x3F94F8, 0x39D520);

        using s_wscrpc_update_t = void (*)(const wchar_t* command, const bool async);
        s_wscrpc_update_t s_wscrpc_update = detail::initializer<s_wscrpc_update_t>(0x2A120, 0x27F50);

        void* proceed_queue = detail::initializer<void*>(0x29C80, 0x27AD0);
        void* proceed_item = detail::initializer<void*>(0x29BA0, 0x278A0);
    } ctx;
} // namespace globals
