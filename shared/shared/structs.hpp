#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>

#include "shared/util.hpp"

namespace shared {
    constexpr std::size_t kMaxNameLength = 128;
    constexpr std::string_view kCtxPath = "ctx.bin";

    namespace detail {
        inline std::string ctx_path() {
            auto path = util::app_path().parent_path();
            path /= kCtxPath;
            return path.string();
        }
    } // namespace detail

    enum class e_state : std::uint8_t {
        ON = 0,
        OFF,
    };

    enum class e_product : std::uint8_t {
        NONE = 0,
        AV = (1 << 0),
        FIREWALL = (1 << 1),
    };
    DEFINE_ENUM_FLAG_OPERATORS(e_product);

    struct init_ctx_t {
    public:
        e_state state = e_state::ON;
        e_product product = e_product::NONE;

        void serialize() const {
            std::ofstream stream(detail::ctx_path(), std::ios::binary);
            stream.write(reinterpret_cast<const char*>(this), sizeof(*this));
        }

        void deserialize() {
            std::ifstream stream(detail::ctx_path(), std::ios::binary);
            stream.read(reinterpret_cast<char*>(this), sizeof(*this));
        }
    };
    static_assert(std::is_trivially_copyable_v<init_ctx_t>);
} // namespace shared

/// Creating custom formatters for the std::format function so that
/// we can easily format stuff
///
template <>
struct std::formatter<shared::e_state, wchar_t> : std::formatter<std::wstring, wchar_t> {
    template <class FormatContextTy>
    constexpr auto format(const shared::e_state& value, FormatContextTy& ctx) const {
        return std::formatter<std::wstring, wchar_t>::format(value == shared::e_state::ON ? L"on" : L"off", ctx);
    }
};
