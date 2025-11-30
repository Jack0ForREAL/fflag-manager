#include "memory.hpp"
#include "constants.hpp"
#include <tlhelp32.h>
#include <iostream>

namespace odessa
{
    // ORIGINAL CONSTRUCTOR (Legacy support)
    c_memory::c_memory( const std::string &name ) noexcept
    {
        // Redirects to the PID finder logic simply
        auto pids = get_all_roblox_pids();
        if (!pids.empty()) {
            m_pid = pids[0];
            m_process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_pid);
        }
    }

    // NEW CONSTRUCTOR: Attach to specific PID
    c_memory::c_memory( std::int32_t pid ) noexcept
    {
        m_pid = pid;
        m_process = OpenProcess( PROCESS_ALL_ACCESS, FALSE, pid );
    }

    c_memory::~c_memory( ) noexcept
    {
        if ( m_process )
        {
            CloseHandle( m_process );
            m_process = nullptr;
        }
        m_pid = 0;
    }

    // NEW: Get all PIDs matching Roblox
    std::vector<std::int32_t> c_memory::get_all_roblox_pids() 
    {
        std::vector<std::int32_t> pids;
        const auto snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
        if ( snapshot == INVALID_HANDLE_VALUE ) return pids;

        PROCESSENTRY32 proc { .dwSize = sizeof( PROCESSENTRY32 ) };

        if ( Process32First( snapshot, &proc ) )
        {
            do
            {
                if ( constants::client_name == proc.szExeFile )
                {
                    pids.push_back(static_cast<std::int32_t>(proc.th32ProcessID));
                }
            } while ( Process32Next( snapshot, &proc ) );
        }
        CloseHandle( snapshot );
        return pids;
    }

    std::unique_ptr< module_t > c_memory::module( const std::string &name ) const noexcept
    {
        if ( !m_process or m_pid == 0 ) return nullptr;

        const auto snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_pid );
        if ( snapshot == INVALID_HANDLE_VALUE ) return nullptr;

        MODULEENTRY32 mod { .dwSize = sizeof( MODULEENTRY32 ) };
        std::unique_ptr< module_t > result;

        if ( Module32First( snapshot, &mod ) )
        {
            do
            {
                if ( name == mod.szModule )
                {
                    result = std::make_unique< module_t >( );
                    result->base = reinterpret_cast< std::uint64_t >( mod.modBaseAddr );
                    result->size = mod.modBaseSize;
                    result->name = mod.szModule;
                    result->path = mod.szExePath;
                    break;
                }
            } while ( Module32Next( snapshot, &mod ) );
        }
        CloseHandle( snapshot );
        return result;
    }

    std::uint64_t c_memory::find( const std::vector< std::uint8_t > &pattern ) const noexcept
    {
        const auto mod = module( constants::client_name );
        if ( !mod or pattern.empty( ) ) return 0;

        std::uint64_t start = mod->base;
        std::uint64_t end   = mod->base + mod->size;
        MEMORY_BASIC_INFORMATION mbi { };

        while ( start < end )
        {
            if ( VirtualQueryEx( m_process, reinterpret_cast< void * >( start ), &mbi, sizeof( mbi ) ) == sizeof( mbi ) )
            {
                bool readable = ( mbi.State == MEM_COMMIT )
                            and ( ( mbi.Protect & PAGE_READONLY ) or ( mbi.Protect & PAGE_READWRITE ) or ( mbi.Protect & PAGE_WRITECOPY )
                                  or ( mbi.Protect & PAGE_EXECUTE_READ ) or ( mbi.Protect & PAGE_EXECUTE_READWRITE ) );

                if ( readable )
                {
                    auto region_size = mbi.RegionSize;
                    if ( start + region_size > end ) region_size = end - start;

                    auto buffer = read( start, region_size );
                    if ( buffer.empty( ) ) { start += region_size; continue; }

                    for ( std::size_t idx = 0; idx + pattern.size( ) <= buffer.size( ); ++idx )
                    {
                        bool match = true;
                        for ( std::size_t pat_idx = 0; pat_idx < pattern.size( ); ++pat_idx )
                        {
                            if ( pattern[ pat_idx ] != 0xcc and buffer[ idx + pat_idx ] != pattern[ pat_idx ] )
                            {
                                match = false; break;
                            }
                        }
                        if ( match ) return start + idx;
                    }
                }
                start += mbi.RegionSize;
            }
            else start += 0x1000;
        }
        return 0;
    }

    std::vector< std::uint64_t > c_memory::find_all( const std::vector< std::uint8_t > &pattern ) const noexcept
    {
        std::vector< std::uint64_t > results;
        const auto mod = module( constants::client_name );
        if ( !mod or pattern.empty( ) ) return results;

        std::uint64_t start = mod->base;
        std::uint64_t end   = mod->base + mod->size;
        MEMORY_BASIC_INFORMATION mbi { };

        while ( start < end )
        {
            if ( VirtualQueryEx( m_process, reinterpret_cast< void * >( start ), &mbi, sizeof( mbi ) ) == sizeof( mbi ) )
            {
                bool readable = ( mbi.State == MEM_COMMIT )
                            and ( ( mbi.Protect & PAGE_READONLY ) or ( mbi.Protect & PAGE_READWRITE ) or ( mbi.Protect & PAGE_WRITECOPY )
                                  or ( mbi.Protect & PAGE_EXECUTE_READ ) or ( mbi.Protect & PAGE_EXECUTE_READWRITE ) );

                if ( readable )
                {
                    auto region_size = mbi.RegionSize;
                    if ( start + region_size > end ) region_size = end - start;

                    auto buffer = read( start, region_size );
                    if ( buffer.empty( ) ) { start += region_size; continue; }

                    for ( std::size_t idx = 0; idx + pattern.size( ) <= buffer.size( ); ++idx )
                    {
                        bool match = true;
                        for ( std::size_t pat_idx = 0; pat_idx < pattern.size( ); ++pat_idx )
                        {
                            if ( pattern[ pat_idx ] != 0xcc and buffer[ idx + pat_idx ] != pattern[ pat_idx ] )
                            {
                                match = false; break;
                            }
                        }
                        if ( match ) results.push_back( start + idx );
                    }
                }
                start += mbi.RegionSize;
            }
            else start += 0x1000;
        }
        return results;
    }

    std::uint64_t c_memory::rebase( const std::uint64_t address, e_rebase_type rebase_type ) const noexcept
    {
        const auto mod = module( constants::client_name );
        if ( !mod ) return 0;
        switch ( rebase_type )
        {
            case e_rebase_type::sub : return address - mod->base;
            case e_rebase_type::add : return mod->base + address;
            default : return 0;
        }
    }
}
