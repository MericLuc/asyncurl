/**
 * Copyright (C) 2023 Osmozis SA - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file mhandle.hpp
 * @brief Wrapper around curl multi handle - It represents a session that can own several (thousands) transfers
 * @see https://everything.curl.dev/libcurl/drive/multi-socket for more informations
 *
 * In a nutshell, here are the key informations to know about the mhandle:
 * <ul>
 * <li>It allows to perform multiple parallel transfers</li>
 * <li>All the transfers are done in a single thread</li>
 * <li>It is driven by a miniloop event-loop (\see https://gitlab.osmoziswifi.com/common/miniloop)</li>
 * </ul>
 * @author lhm
 */

#ifndef INCLUDE_ASYNCURL_MHANDLE_H
#define INCLUDE_ASYNCURL_MHANDLE_H

#include <any>
#include <cstddef>    // size_t
#include <cstdint>    // int64_t
#include <functional> // std::function
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include <Loop.h>

template<class T>
using uptr = std::unique_ptr<T>;

namespace asyncurl
{
class handle;

/*********************************************************************************************************************/
class mhandle
{
public:
    using TCbError = std::function<void(int)>;

    /*!
     * @brief MHDL_RetCode describes the return codes of the asyncurl::mhandle class methods
     */
    typedef enum
    {
        MHDL_OK = 0,         /*!< OK */
        MHDL_BAD_PARAM,      /*!< An invalid parameter was passed to a function  */
        MHDL_ADD_OWNED,      /*!< An handle who is owned by another mhandle was attempted to get added */
        MHDL_ADD_ALREADY,    /*!< An handle already owned was attempted to get added again */
        MHDL_REMOVE_OWNED,   /*!< An handle who is owned by another mhandle was attempted to get removed */
        MHDL_REMOVE_ALREADY, /*!< An handle already removed (or never added) was attempted to get removed again */
        MHDL_BAD_HANDLE,     /*!< An handle passed-in is not a valid handle */
        MHDL_OUT_OF_MEM,     /*!< An dynamic allocation call failed (you were probably too greedy) */
        MHDL_INTERNAL_ERROR  /*!< Internal error */
    } MHDL_RetCode;

private:
    void*                                 curl_multi__{ nullptr }; /*!< Raw curl multi-handle (CURL::CURLM) */
    std::map<void*, handle*>              handles__{};             /*!< Pool of the single transfers */
    std::map<void*, uptr<loop::Loop::IO>> ios__{};                 /*!< Pool of IOs */
    int running_handles__{ 0 }; /*!< Number of running transfers - or -1 in case of stop */

    TCbError cb_error__{};

    loop::Loop& loop__;

    uptr<loop::Loop::Timeout> timeout__{ nullptr };

    mhandle(const mhandle&) = delete;
    mhandle& operator=(const mhandle&) = delete;
    mhandle(mhandle&&)                 = delete;
    mhandle& operator=(mhandle&&) = delete;

    static int timer_callback(void*, long, void*);
    static int socket_callback(void*, size_t, int, void*, void*);

protected:
    MHDL_RetCode set_opt_long(int id, long val) noexcept;
    MHDL_RetCode set_opt_ptr(int id, const void* val) noexcept;
    MHDL_RetCode set_opt_bool(int id, bool val) noexcept;
    MHDL_RetCode set_opt_offset(int id, long val) noexcept;

    void handle_stop(int) noexcept;
    void handle_msgs(void) noexcept;

public:
    mhandle(loop::Loop&);
    ~mhandle() noexcept;

    MHDL_RetCode add_handle(handle&) noexcept;
    MHDL_RetCode remove_handle(handle&) noexcept;
    auto         enumerate_added_handles(void) const noexcept { return std::size(handles__); }
    auto         enumerate_running_handles(void) const noexcept { return running_handles__; }

    void set_cb_error(TCbError&) noexcept;

    MHDL_RetCode set_opt(int id, std::any val) noexcept;

    // Convenience methods used for setting options
    MHDL_RetCode set_max_concurrent_streams(long) noexcept;
    MHDL_RetCode set_max_host_connections(long) noexcept;
    MHDL_RetCode set_max_pipeline_length(long) noexcept;
    MHDL_RetCode set_max_total_connections(long) noexcept;
    MHDL_RetCode set_maxconnects(long) noexcept;
    MHDL_RetCode set_pipelining(long) noexcept;
    //----------------------------------------------//

    void* raw(void) noexcept;

    static std::string_view retCode2Str(MHDL_RetCode) noexcept;
};

} // namespace asyncurl

#endif // INCLUDE_ASYNCURL_MHANDLE_H
