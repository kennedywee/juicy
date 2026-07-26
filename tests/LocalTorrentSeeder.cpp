#include <atomic>
#include <chrono>
#include <climits>
#include <cstddef>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_flags.hpp>
#include <libtorrent/torrent_info.hpp>

namespace lt = libtorrent;

namespace {

std::atomic_bool keepRunning = true;

void stopSeeder(int)
{
    keepRunning.store(false);
}

} // namespace

int main(int argc, char *argv[])
{
    if (argc < 2 || argc > 4) {
        std::cerr << "usage: juicy-local-seeder VIDEO_FILE [PORT] [UPLOAD_KIB_PER_SECOND]\n";
        return EXIT_FAILURE;
    }

    const std::filesystem::path videoPath = std::filesystem::absolute(argv[1]);
    if (!std::filesystem::is_regular_file(videoPath)) {
        std::cerr << "video file does not exist: " << videoPath << '\n';
        return EXIT_FAILURE;
    }

    int port = 16881;
    if (argc >= 3) {
        port = std::stoi(argv[2]);
    }
    if (port <= 0 || port > 65535) {
        std::cerr << "invalid port\n";
        return EXIT_FAILURE;
    }
    int uploadKib = 0;
    if (argc == 4) {
        uploadKib = std::stoi(argv[3]);
    }
    if (uploadKib < 0 || uploadKib > INT_MAX / 1024) {
        std::cerr << "invalid upload rate\n";
        return EXIT_FAILURE;
    }

    std::vector<lt::create_file_entry> files;
    files.emplace_back(videoPath.filename().string(), std::filesystem::file_size(videoPath));
    lt::create_torrent creator(std::move(files), 16 * 1024, lt::create_torrent::v1_only);
    creator.set_creator("Juicy local integration test");

    lt::error_code error;
    lt::set_piece_hashes(creator, videoPath.parent_path().string(), error);
    if (error) {
        std::cerr << "hashing failed: " << error.message() << '\n';
        return EXIT_FAILURE;
    }

    const std::vector<char> torrentBuffer = creator.generate_buf();
    auto information = std::make_shared<lt::torrent_info>(
        lt::span<char const>(
            torrentBuffer.data(),
            static_cast<std::ptrdiff_t>(torrentBuffer.size())
        ),
        lt::from_span
    );
    lt::settings_pack settings;
    settings.set_str(
        lt::settings_pack::listen_interfaces,
        "127.0.0.1:" + std::to_string(port)
    );
    settings.set_bool(lt::settings_pack::enable_dht, false);
    settings.set_bool(lt::settings_pack::enable_lsd, false);
    settings.set_bool(lt::settings_pack::enable_upnp, false);
    settings.set_bool(lt::settings_pack::enable_natpmp, false);
    if (uploadKib > 0) {
        settings.set_int(lt::settings_pack::upload_rate_limit, uploadKib * 1024);
    }
    lt::session session(settings);
    if (uploadKib > 0) {
        lt::peer_class_info localClass = session.get_peer_class(
            lt::session_handle::local_peer_class_id
        );
        localClass.upload_limit = uploadKib * 1024;
        session.set_peer_class(lt::session_handle::local_peer_class_id, localClass);
    }

    lt::add_torrent_params parameters;
    parameters.ti = information;
    parameters.save_path = videoPath.parent_path().string();
    parameters.flags |= lt::torrent_flags::seed_mode;
    std::string magnet = lt::make_magnet_uri(parameters);
    const lt::torrent_handle handle = session.add_torrent(parameters, error);
    if (error || !handle.is_valid()) {
        std::cerr << "adding seed failed: " << error.message() << '\n';
        return EXIT_FAILURE;
    }

    magnet += "&x.pe=127.0.0.1:" + std::to_string(port);
    std::cout << magnet << std::endl;

    std::signal(SIGINT, stopSeeder);
    std::signal(SIGTERM, stopSeeder);
    while (keepRunning.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return EXIT_SUCCESS;
}
