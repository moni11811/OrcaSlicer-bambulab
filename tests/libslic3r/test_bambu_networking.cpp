#include <catch2/catch_all.hpp>

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

#include "slic3r/Utils/bambu_networking.hpp"
#include "slic3r/Utils/PJarczakLinuxBridge/PJarczakLinuxBridgeConfig.hpp"

using namespace Slic3r;

TEST_CASE("extract_base_version", "[BambuNetworking]") {
    SECTION("version without suffix returns unchanged") {
        REQUIRE(extract_base_version("02.03.00.62") == "02.03.00.62");
        REQUIRE(extract_base_version("01.00.00.00") == "01.00.00.00");
    }

    SECTION("version with suffix returns base only") {
        REQUIRE(extract_base_version("02.03.00.62-mod") == "02.03.00.62");
        REQUIRE(extract_base_version("02.03.00.62-patched") == "02.03.00.62");
        REQUIRE(extract_base_version("02.03.00.62-test-build") == "02.03.00.62");
    }

    SECTION("empty string returns empty") {
        REQUIRE(extract_base_version("") == "");
    }

    SECTION("suffix only returns empty") {
        REQUIRE(extract_base_version("-mod") == "");
    }
}

TEST_CASE("extract_suffix", "[BambuNetworking]") {
    SECTION("version without suffix returns empty") {
        REQUIRE(extract_suffix("02.03.00.62") == "");
        REQUIRE(extract_suffix("01.00.00.00") == "");
    }

    SECTION("version with suffix returns suffix without dash") {
        REQUIRE(extract_suffix("02.03.00.62-mod") == "mod");
        REQUIRE(extract_suffix("02.03.00.62-patched") == "patched");
    }

    SECTION("version with multiple dashes returns everything after first dash") {
        REQUIRE(extract_suffix("02.03.00.62-test-build") == "test-build");
    }

    SECTION("empty string returns empty") {
        REQUIRE(extract_suffix("") == "");
    }

    SECTION("suffix only returns suffix without leading dash") {
        REQUIRE(extract_suffix("-mod") == "mod");
    }
}

TEST_CASE("NetworkLibraryVersionInfo::from_static", "[BambuNetworking]") {
    SECTION("converts static version info correctly") {
        NetworkLibraryVersion static_ver{"02.03.00.62", "02.03.00.62", nullptr, true, nullptr};
        auto info = NetworkLibraryVersionInfo::from_static(static_ver);

        REQUIRE(info.version == "02.03.00.62");
        REQUIRE(info.base_version == "02.03.00.62");
        REQUIRE(info.suffix == "");
        REQUIRE(info.display_name == "02.03.00.62");
        REQUIRE(info.url_override == "");
        REQUIRE(info.is_latest == true);
        REQUIRE(info.warning == "");
        REQUIRE(info.is_discovered == false);
    }

    SECTION("handles version with warning") {
        NetworkLibraryVersion static_ver{"02.00.02.50", "02.00.02.50", nullptr, false, "This is a warning"};
        auto info = NetworkLibraryVersionInfo::from_static(static_ver);

        REQUIRE(info.version == "02.00.02.50");
        REQUIRE(info.is_latest == false);
        REQUIRE(info.warning == "This is a warning");
        REQUIRE(info.is_discovered == false);
    }

    SECTION("handles version with url override") {
        NetworkLibraryVersion static_ver{"02.01.01.52", "02.01.01.52", "https://custom.url/plugin.zip", false, nullptr};
        auto info = NetworkLibraryVersionInfo::from_static(static_ver);

        REQUIRE(info.url_override == "https://custom.url/plugin.zip");
    }
}

TEST_CASE("NetworkLibraryVersionInfo::from_discovered", "[BambuNetworking]") {
    SECTION("creates discovered version info correctly") {
        auto info = NetworkLibraryVersionInfo::from_discovered("02.03.00.62-mod", "02.03.00.62", "mod");

        REQUIRE(info.version == "02.03.00.62-mod");
        REQUIRE(info.base_version == "02.03.00.62");
        REQUIRE(info.suffix == "mod");
        REQUIRE(info.display_name == "02.03.00.62-mod");
        REQUIRE(info.url_override == "");
        REQUIRE(info.is_latest == false);
        REQUIRE(info.warning == "");
        REQUIRE(info.is_discovered == true);
    }
}

#if defined(__APPLE__) || defined(__WXMAC__)
TEST_CASE("macOS Linux bridge stays enabled by default", "[BambuNetworking][PJarczakLinuxBridge]") {
    struct EnvGuard {
        const char* previous = std::getenv("PJARCZAK_LINUX_BRIDGE_ENABLED");
        std::string previous_value = previous ? previous : "";
        ~EnvGuard()
        {
            if (previous)
                setenv("PJARCZAK_LINUX_BRIDGE_ENABLED", previous_value.c_str(), 1);
            else
                unsetenv("PJARCZAK_LINUX_BRIDGE_ENABLED");
        }
    } env_guard;

    unsetenv("PJARCZAK_LINUX_BRIDGE_ENABLED");
    REQUIRE(PJarczakLinuxBridge::enabled());
    REQUIRE(PJarczakLinuxBridge::use_bridge_network_module());
    REQUIRE(PJarczakLinuxBridge::source_module_is_network_module());
    REQUIRE(PJarczakLinuxBridge::should_force_linux_plugin_payload("plugins"));

    setenv("PJARCZAK_LINUX_BRIDGE_ENABLED", "0", 1);
    REQUIRE_FALSE(PJarczakLinuxBridge::enabled());
    REQUIRE_FALSE(PJarczakLinuxBridge::use_bridge_network_module());
    REQUIRE_FALSE(PJarczakLinuxBridge::source_module_is_network_module());
    REQUIRE_FALSE(PJarczakLinuxBridge::should_force_linux_plugin_payload("plugins"));
}

TEST_CASE("macOS Linux bridge can be forced on", "[BambuNetworking][PJarczakLinuxBridge]") {
    struct EnvGuard {
        const char* previous = std::getenv("PJARCZAK_LINUX_BRIDGE_ENABLED");
        std::string previous_value = previous ? previous : "";
        ~EnvGuard()
        {
            if (previous)
                setenv("PJARCZAK_LINUX_BRIDGE_ENABLED", previous_value.c_str(), 1);
            else
                unsetenv("PJARCZAK_LINUX_BRIDGE_ENABLED");
        }
    } env_guard;

    setenv("PJARCZAK_LINUX_BRIDGE_ENABLED", "1", 1);
    REQUIRE(PJarczakLinuxBridge::enabled());
    REQUIRE(PJarczakLinuxBridge::use_bridge_network_module());
    REQUIRE(PJarczakLinuxBridge::source_module_is_network_module());
    REQUIRE(PJarczakLinuxBridge::should_force_linux_plugin_payload("plugins"));
}

TEST_CASE("macOS Linux bridge installer mounts runtime payload into Lima", "[BambuNetworking][macOS][PJarczakLinuxBridge]") {
    const std::string install_script_path =
        std::string(ORCASLICER_SOURCE_DIR) + "/tools/pjarczak_bambu_runtime/macos/pjarczak_install_macos_runtime.sh";
    std::ifstream install_script(install_script_path);
    REQUIRE(install_script.good());

    const std::string source((std::istreambuf_iterator<char>(install_script)), std::istreambuf_iterator<char>());
    REQUIRE(source.find("--mount-only") != std::string::npos);
    REQUIRE(source.find("$APP_SUPPORT_DIR:w") != std::string::npos);
}

TEST_CASE("macOS Linux bridge verifier checks payload inside Lima guest", "[BambuNetworking][macOS][PJarczakLinuxBridge]") {
    const std::string verify_script_path =
        std::string(ORCASLICER_SOURCE_DIR) + "/tools/pjarczak_bambu_runtime/macos/pjarczak_verify_macos_runtime.sh";
    std::ifstream verify_script(verify_script_path);
    REQUIRE(verify_script.good());

    const std::string source((std::istreambuf_iterator<char>(verify_script)), std::istreambuf_iterator<char>());
    REQUIRE(source.find("check_lima_payload_mount") != std::string::npos);
    REQUIRE(source.find("cannot access runtime payload") != std::string::npos);
}

TEST_CASE("macOS app exit shuts down Bambu networking before wx exit", "[BambuNetworking][macOS][shutdown]") {
    const std::string gui_app_path = std::string(ORCASLICER_SOURCE_DIR) + "/src/slic3r/GUI/GUI_App.cpp";
    std::ifstream gui_app(gui_app_path);
    REQUIRE(gui_app.good());

    const std::string source((std::istreambuf_iterator<char>(gui_app)), std::istreambuf_iterator<char>());
    const auto on_exit_pos = source.find("int GUI_App::OnExit()");
    REQUIRE(on_exit_pos != std::string::npos);

    const auto next_function_pos = source.find("\nclass wxBoostLog", on_exit_pos);
    REQUIRE(next_function_pos != std::string::npos);

    const auto on_exit_body = source.substr(on_exit_pos, next_function_pos - on_exit_pos);
    const auto delete_agent_pos = on_exit_body.find("delete m_agent;");
    const auto shutdown_pos = on_exit_body.find("BBLNetworkPlugin::shutdown();");
    const auto wx_exit_pos = on_exit_body.find("return wxApp::OnExit();");

    REQUIRE(delete_agent_pos != std::string::npos);
    REQUIRE(shutdown_pos != std::string::npos);
    REQUIRE(wx_exit_pos != std::string::npos);
    REQUIRE(delete_agent_pos < shutdown_pos);
    REQUIRE(shutdown_pos < wx_exit_pos);
}
#endif
