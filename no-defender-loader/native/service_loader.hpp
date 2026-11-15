#pragma once
#include "registry.hpp"

#include <expected>
#include <filesystem>
#include <memory>
#include <winternl.h>

namespace service_loader {
    using error_t = std::monostate;

    class Instance {
    public:
        using handle_t = std::unique_ptr<std::remove_pointer_t<SC_HANDLE>, decltype(&CloseServiceHandle)>;

        Instance(): _handle(handle_t(OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS), &CloseServiceHandle)) { }

        [[nodiscard]] std::expected<handle_t, error_t> create_and_start_um_service(const std::string_view name, const std::string_view display_name,
                                                                                   const std::string& path) {
            SC_HANDLE result = nullptr;

            /// Retry a few times
            for (std::size_t i = 0; i < 5 && result == nullptr; ++i) {
                result = CreateServiceA(_handle.get(), name.data(), display_name.data(), SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
                                        SERVICE_ERROR_NORMAL, path.data(), nullptr, nullptr, nullptr, nullptr, nullptr);

                if (result == nullptr) {
                    /// \todo @es3n1n: Check whether the path to its binary is the one we are expecting
                    auto handle = open_service(name);
                    if (!handle.has_value()) {
                        continue;
                    }

                    if (!stop_and_delete_service(*handle)) {
                        return std::unexpected(error_t{});
                    }

                    handle->reset();
                    continue;
                }
            }

            if (result == nullptr) {
                return std::unexpected(error_t{});
            }

            if (!static_cast<bool>(StartServiceA(result, 0, nullptr)) && GetLastError() != ERROR_SERVICE_ALREADY_RUNNING) {
                return std::unexpected(error_t{});
            }

            return handle_t(result, &CloseServiceHandle);
        }

        [[nodiscard]] std::expected<handle_t, error_t> open_service(const std::string_view name) {
            auto result = OpenServiceA(_handle.get(), name.data(), SERVICE_ALL_ACCESS);
            if (result == nullptr) {
                return std::unexpected(error_t{});
            }

            return handle_t(result, &CloseServiceHandle);
        }

        bool stop_and_delete_service(handle_t& service) {
            SERVICE_STATUS status = {};
            ControlService(service.get(), SERVICE_CONTROL_STOP, &status);
            return static_cast<bool>(DeleteService(service.get()));
        }

    private:
        handle_t _handle;
    };
} // namespace service_loader
