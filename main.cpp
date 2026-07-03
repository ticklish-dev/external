#include "include.hxx"
#include <features/config/config.h>

int main()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Load external config (rebranding) first
    external_config::load();

    printf("=== %s ===\n\n", external_config::cheat_name.c_str());

    static const char* BINARY_NAME = ("RobloxPlayerBeta.exe");

    printf("Waiting for Roblox...\n");
    while (!memory->find_process_id(BINARY_NAME))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    printf("Roblox found!\n");

    while (!memory->attach_to_process(BINARY_NAME))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    printf("Attached to Roblox!\n");

    while (!memory->find_module_address(BINARY_NAME))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    printf("Module found!\n");

    settings::globals::version_matched = true;

    // Wait for game to fully load (DataModel + Players must be valid)
    printf("Waiting for game to load...\n");
    std::uint64_t fake_datamodel = 0;
    int dbg_tick = 0;
    while (true)
    {
		auto module = memory->get_module_address();
        auto fake_datamodel = memory->read<std::uint64_t>(module + Offsets::FakeDataModel::Pointer);

        if (dbg_tick % 10 == 0)
        {
            printf("[DBG] module=0x%llX  FakeDataModel ptr=0x%llX\n",
                (unsigned long long)memory->get_module_address(),
                (unsigned long long)fake_datamodel);
        }

        if (fake_datamodel != 0)
        {
            std::uint64_t real_dm = memory->read<std::uint64_t>(fake_datamodel + Offsets::FakeDataModel::RealDataModel);
            game::datamodel = rbx::instance_t(real_dm);

            if (dbg_tick % 10 == 0)
                printf("[DBG] RealDataModel=0x%llX\n", (unsigned long long)real_dm);

            if (game::datamodel.address != 0)
            {
                game::players = game::datamodel.find_first_child_by_class("Players");

                if (dbg_tick % 10 == 0)
                    printf("[DBG] Players=0x%llX\n", (unsigned long long)game::players.address);

                if (game::players.address != 0)
                    break;
            }
        }

        dbg_tick++;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    game::visengine = {memory->read<std::uint64_t>(memory->get_module_address() + Offsets::VisualEngine::Pointer)};
    printf("Game loaded!\n");

    std::thread(cache::run).detach();
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    if (!InitializeStorage())
    {
        printf("failed to initialize storage\n");
        printf("\nPress ENTER to exit...\n");
        std::cin.get();
        return 1;
    }

    std::thread(AutoRescanHandler).detach();
    std::thread(aimbot::run).detach();
    std::thread(silentaim::run).detach();
    std::thread(rage::hitsounds_detector_thread).detach();
    std::thread(rage::hittracers_detector_thread).detach();
    std::thread(rage::orbit::run).detach();
    std::thread(rage::rapidfire::run).detach();
    std::thread(rage::hitbox_expander::run).detach();
    std::thread(rage::spin360::run).detach();
    std::thread(rage::noclip::run).detach();
    std::thread(rage::hipheight::run).detach();
    std::thread(movement::run).detach();
    std::thread(movement::gravity::run).detach();
    std::thread(lighting::fog::run).detach();
    std::thread(lighting::exposure::run).detach();
    std::thread(lighting::clocktime::run).detach();
    std::thread(lighting::shadows::run).detach();
    std::thread(lighting::skybox::run).detach();
    std::thread(exploits::headless::run).detach();
    std::thread(exploits::korblox::run).detach();
    std::thread(exploits::desync::run).detach();
    std::thread(exploits::displayfps::run).detach();
    std::thread(exploits::antiafk::run).detach();
    std::thread(exploits::fpscaps::run).detach();
    std::thread(exploits::freezeplayer::run).detach();
    std::thread(menu::console::run).detach();
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    // Autoload config if set
    if (!external_config::autoload_config.empty())
    {
        if (config::config_exists(external_config::autoload_config))
        {
            config::load_config(external_config::autoload_config);
            printf("Autoloaded config: %s\n", external_config::autoload_config.c_str());
        }
        else
        {
            printf("Autoload config '%s' not found, using defaults\n", external_config::autoload_config.c_str());
        }
    }

    if (!render->create_window())
    {
        printf("failed to create window\n");
        printf("\nPress ENTER to exit...\n");
        std::cin.get();
        return 1;
    }

    if (!render->create_device())
    {
        printf("failed to create device\n");
        printf("\nPress ENTER to exit...\n");
        std::cin.get();
        return 1;
    }

    if (!render->create_imgui())
    {
        printf("failed to create imgui\n");
        printf("\nPress ENTER to exit...\n");
        std::cin.get();
        return 1;
    }

    if (!Menu::Initialize(render->detail->window, render->detail->device, render->detail->device_context))
    {
        printf("failed to initialize menu\n");
        printf("\nPress ENTER to exit...\n");
        std::cin.get();
        return 1;
    }

    static auto last_pid_check = std::chrono::steady_clock::now();

    while (true)
    {
        // Check if Roblox is still running
        if (std::chrono::steady_clock::now() - last_pid_check > std::chrono::milliseconds(500))
        {
            DWORD exit_code = 0;
            HANDLE proc = memory->get_process_handle();
            if (proc == nullptr || proc == INVALID_HANDLE_VALUE ||
                (GetExitCodeProcess(proc, &exit_code) && exit_code != STILL_ACTIVE) ||
                memory->find_process_id(BINARY_NAME) == 0)
            {
                Menu::Shutdown();
                render->running = false;
                std::exit(0);
            }
            last_pid_check = std::chrono::steady_clock::now();
        }

        render->start_render();

        if (!should_render_ui())
        {
            render->end_render();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        render->render_visuals();

        {
            const std::uint64_t cur_game_id = game::datamodel.address
                ? memory->read<std::uint64_t>(game::datamodel.address + Offsets::DataModel::GameId) : 0ULL;
            if (cur_game_id == gamesupport::game_ids::BladeBall)
                bladeball::update();
            else
                bladeball::stop();
        }

        if (render->running)
        {
            render->render_menu();
        }

        render->end_render();

        if (settings::menu::performance_mode)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    return 0;
}
