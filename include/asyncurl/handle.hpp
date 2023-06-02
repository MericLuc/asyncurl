/**
 * Copyright (C) 2023 Osmozis SA - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file handle.hpp
 * @brief Wrapper around curl easy handle
 * \see https://everything.curl.dev/libcurl/easyhandle for more informations
 * Basically, this is a handle to a transfer.
 * It provides functionalities such as :
 * <ul>
 * <li> Control over how the upcoming transfer will be performed </li>
 * <li> Means to register custom callbacks (\see https://everything.curl.dev/libcurl/callbacks) </li>
 * <li> Reusability and easy duplication options </li>
 * </ul>
 * @author lhm
 */

#ifndef INCLUDE_ASYNCURL_HANDLE_H
#define INCLUDE_ASYNCURL_HANDLE_H

#include <cstddef>    // size_t
#include <cstdint>    // int64_t
#include <functional> // std::function
#include <string>

namespace asyncurl
{
class mhandle;

/*********************************************************************************************************************/
class handle
{
public:
    using TCbWrite    = std::function<size_t(char*, size_t, size_t, void*)>;
    using TCbRead     = std::function<size_t(char*, size_t, size_t, void*)>;
    using TCbProgress = std::function<int(void*, int64_t, int64_t, int64_t, int64_t)>; // curl_off_t <- int64_t
    using TCbHeader   = std::function<size_t(char*, size_t, size_t, void*)>;
    using TCbDebug    = std::function<int(void*, void*, char*, size_t, void*)>;

private:
    const mhandle* multi_handler__{ nullptr };
    void*          curl_handle__{ nullptr }; /*< CURL easy handle - you do NOT want to mess with this */
    int            flags__{ 0 };

    TCbWrite    cb_write__{};
    TCbRead     cb_read__{};
    TCbProgress cb_progress__{};
    TCbHeader   cb_header__{};
    TCbDebug    cb_debug__{};

protected:
    long        get_info_long(int) const;
    uint64_t    get_info_socket(int) const;
    double      get_info_double(int) const;
    std::string get_info_string(int) const;

    void set_option_long(int id, long val);
    void set_option_offset(int id, long val);
    void set_option_ptr(int id, const void* val);
    void set_option_string(int id, const char* val);
    void set_option_bool(int id, bool val);

public:
    handle();
    ~handle() noexcept;
    handle(const handle&) = delete;
    handle& operator=(const handle&) = delete;
    handle(handle&&)                 = delete;
    handle& operator=(handle&&) = delete;

    bool pause(int bitmask) noexcept;
    bool unpause(int bitmask) noexcept;
    bool is_paused(int bitmask) noexcept;

    double      get_total_time() const;
    double      get_namelookup_time() const;
    double      get_connect_time() const;
    double      get_retransfer_time() const;
    double      get_size_download() const;
    double      get_speed_download() const;
    double      get_size_upload() const;
    double      get_speed_upload() const;
    long        get_response_code() const;
    std::string get_url() const;
};

} // namespace asyncurl

#endif // INCLUDE_ASYNCURL_HANDLE_H
