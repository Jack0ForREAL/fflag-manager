#include "native.hpp"
#include "constants.hpp"
#include "memory/memory.hpp"
#include "fflags/fflags.hpp"
#include "engine/engine.hpp"

#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

void SetColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void DrawHeader() {
    SetColor(11); // Cyan
    std::cout << R"(
   __  __      _                           _ 
  |  \/  | ___| |_  __ _   __ __  _ _  ___| |
  | |\/| |/ -_)  _|/ _` | / _/ _|| '_|/ -_)_|
  |_|  |_|\___|\__|\__,_| \__\__||_|  \___(_)
       FFLAG MANAGER [MULTI-INSTANCE]
)" << std::endl;
    SetColor(7); // Default
    std::cout << "========================================" << std::endl;
}

std::int32_t main()
{
    SetConsoleTitleA("FFLAG MANAGER - MULTI INSTANCE FARM");
    DrawHeader();

    while (true) {
        // 1. Get all running Roblox PIDs
        std::vector<std::int32_t> pids = odessa::c_memory::get_all_roblox_pids();

        if (pids.empty()) {
            std::cout << "\r[*] Waiting for Roblox... (0 found)   ";
            Sleep(1000);
            continue;
        }

        std::cout << "\n[+] Found " << pids.size() << " instances. Applying flags...\n";

        // 2. Loop through every single account
        for (const auto& pid : pids) {
            try {
                // Attach to this specific PID
                odessa::g_memory = std::make_unique<odessa::c_memory>(pid);
                
                if (!odessa::g_memory->handle()) {
                    SetColor(12); // Red
                    std::cout << " [!] Failed to attach to PID: " << pid << std::endl;
                    SetColor(7);
                    continue;
                }

                // Initialize FFlag Engine for this specific process
                // (This will scan memory for this specific PID)
                odessa::engine::g_fflags = std::make_unique<odessa::engine::c_fflags>();

                // Apply the JSON
                std::cout << " [>] Applying to PID: " << pid << "... ";
                odessa::engine::setup(); // This reads fflags.json and applies it
                
            } catch (...) {
                SetColor(12);
                std::cout << "Error processing PID " << pid << std::endl;
                SetColor(7);
            }
        }

        SetColor(10); // Green
        std::cout << "\n[+] Cycle complete. Sleeping for 10 seconds..." << std::endl;
        SetColor(7);
        
        // Wait before scanning again (to save CPU)
        Sleep(10000);
    }

    return 0;
}
