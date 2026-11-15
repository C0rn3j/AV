#include <cassert>
#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>
#include <thread>
#include <Windows.h>

#include "native/registry.hpp"
#include "native/service_loader.hpp"
#include "util.hpp"

#include <argparse/argparse.hpp>
#include <shared/structs.hpp>

namespace {
    void setup_registry() {
        auto reg = native::Registry(HKEY_LOCAL_MACHINE, "SOFTWARE");
        reg = reg.create_key("Avast Software");
        reg = reg.create_key("Avast");
        (void)reg.create_key("properties");

        const auto path = util::app_path().parent_path().wstring();
        const bool status = reg.set_value(L"ProgramFolder", path.c_str(), path.length());
        if (!status) {
            throw std::runtime_error("unable to init registry, are we really elevated?");
        }
    }

    void start(service_loader::Instance& loader, std::string& name) {
        auto wsc_proxy = util::app_path().parent_path();
        wsc_proxy /= "wsc_proxy.exe";

        std::cout << "** loading the wsc_proxy" << std::endl;
        auto svc = loader.create_and_start_um_service("wsc_proxy", "wsc_proxy",
                                                      std::format("\"{}\" /runassvc /rpcserver /wsc_name:\"{}\"", wsc_proxy.string(), name));
        if (!svc.has_value()) {
            throw std::runtime_error("unable to load wsc_proxy, please try again");
        }

        std::cout << "** waiting for wsc_proxy (this can take some time)" << std::endl;
        while (util::process_exists(wsc_proxy.filename().wstring())) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    void remove_service(service_loader::Instance& loader) {
        std::cout << "** removing the service" << std::endl;
        auto svc = loader.open_service("wsc_proxy");
        if (!svc.has_value()) {
            throw std::runtime_error("unable to open wsc_proxy");
        }

        if (!loader.stop_and_delete_service(svc.value())) {
            throw std::runtime_error("unable to remove wsc_proxy");
        }
    }
} // namespace

int main(int argc, char* argv[]) try {
    argparse::ArgumentParser program("no-defender-loader", "1.0.1");
    program.add_argument("--disable").help("re-enable firewall/defender").flag();
    program.add_argument("--firewall").help("disable the firewall").flag();
    program.add_argument("--av").help("disable the defender").flag();
    program.add_argument("--name").help("av name").default_value(std::string("Sexy Scolopendra")).nargs(1);
    program.parse_args(argc, argv);

    auto name = program.get<std::string>("--name");
    if (name.length() > shared::kMaxNameLength) {
        throw std::runtime_error(std::format("Max name length is {} characters", shared::kMaxNameLength));
    }

    shared::init_ctx_t ctx = {};
    ctx.state = program.get<bool>("--disable") == true ? shared::e_state::OFF : shared::e_state::ON;

    if (program.get<bool>("--firewall")) {
        ctx.product |= shared::e_product::FIREWALL;
    }

    if (program.get<bool>("--av")) {
        ctx.product |= shared::e_product::AV;
    }

    /// Defaults to the av
    if (ctx.product == shared::e_product::NONE) {
        ctx.product = shared::e_product::AV;
    }

    std::cout << "** saving the ctx.." << std::endl;
    ctx.serialize();

    std::cout << "** setting the registry keys up" << std::endl;
    setup_registry();

    if (!util::grant_privileges({L"SeLoadDriverPrivilege"})) {
        throw std::runtime_error("unable to acquire privileges");
    }

    service_loader::Instance loader = {};

    /// Invoke the wsc_proxy and let it enable/disable stuff
    start(loader, name);

    /// We don't want to add ourselves to autorun in that case
    if (ctx.state == shared::e_state::OFF) {
        remove_service(loader);
    }

    std::cout << "** done!" << std::endl << std::endl;

    if (ctx.state == shared::e_state::ON) {
        std::cout << "** please don't remove any files from this folder, otherwise no-defender wouldn't be activated after the reboot!" << std::endl;
        std::cout << "** if you wish to change the folder, please de-activate no-defender, change the folder and activate it again" << std::endl;
        std::cout << "** to de-activate the no-defender please run 'no_defender.exe --disable'" << std::endl;
    }

    std::cin.get();
    return EXIT_SUCCESS;
} catch (const std::exception& err) {
    std::cerr << err.what() << std::endl;
    std::cin.get();
    std::exit(EXIT_FAILURE);
}
