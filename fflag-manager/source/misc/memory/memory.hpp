#pragma once

#include "native.hpp"
#include <vector> // Added for vector

namespace odessa
{
    enum class e_rebase_type : std::uint8_t
    {
        sub = 0,
        add = 1
    };

    struct module_t
    {
        std::uint64_t base { 0 };
        std::uint32_t size { 0 };

        std::string name { "" };
        std::string path { "" };
    };

    class c_memory
    {
        HANDLE       m_process { nullptr };
        std::int32_t m_pid { 0 };

      public:
        // ORIGINAL: Find by name (kept for compatibility)
        c_memory( const std::string &name ) noexcept;

        // NEW: Attach to specific PID
        c_memory( std::int32_t pid ) noexcept;

        ~c_memory( ) noexcept;

        // NEW: Static helper to get ALL Roblox PIDs
        static std::vector<std::int32_t> get_all_roblox_pids();

        std::unique_ptr< module_t > module( const std::string &name ) const noexcept;
        std::uint64_t find( const std::vector< std::uint8_t > &pattern ) const noexcept;
        std::vector< std::uint64_t > find_all( const std::vector< std::uint8_t > &pattern ) const noexcept;
        std::uint64_t rebase( const std::uint64_t address, e_rebase_type rebase_type = e_rebase_type::sub ) const noexcept;

        template < typename type_t >
        [[nodiscard]] type_t read( const std::uint64_t address ) const noexcept
        {
            type_t buffer { };
            ReadProcessMemory( m_process, reinterpret_cast< void * >( address ), &buffer, sizeof( type_t ), nullptr );
            return buffer;
        }

        [[nodiscard]] std::vector< std::uint8_t > read( const std::uint64_t address, std::size_t size ) const noexcept
        {
            if ( size == 0 ) return { };
            std::vector< std::uint8_t > buffer( size );
            std::size_t bytes_read { 0 };
            if ( !ReadProcessMemory( m_process, reinterpret_cast< void * >( address ), buffer.data( ), size, &bytes_read ) )
                return { };
            if ( bytes_read < size ) buffer.resize( bytes_read );
            return buffer;
        }

        template < typename type_t >
        [[nodiscard]] bool write( const std::uint64_t address, const type_t &value ) const noexcept
        {
            return WriteProcessMemory( m_process, reinterpret_cast< void * >( address ), &value, sizeof( type_t ), nullptr ) != 0;
        }

        template < typename type_t >
        [[nodiscard]] bool write( const std::uint64_t address, const type_t &value, std::size_t size ) const noexcept
        {
            return WriteProcessMemory( m_process, reinterpret_cast< void * >( address ), &value, size, nullptr ) != 0;
        }

        [[nodiscard]] HANDLE handle( ) const noexcept { return m_process; }
        [[nodiscard]] std::int32_t pid( ) const noexcept { return m_pid; }
    };

    inline auto g_memory { std::unique_ptr< c_memory > {} };
}
