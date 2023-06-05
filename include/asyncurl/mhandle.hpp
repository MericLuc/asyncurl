/**
 * Copyright (C) 2023 Osmozis SA - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file mhandle.hpp
 * @brief Wrapper around curl multi handle
 * \see https://everything.curl.dev/libcurl/drive/multi-socket for more informations
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

#include <cstddef>    // size_t
#include <cstdint>    // int64_t
#include <functional> // std::function
#include <string>

namespace loop {
    class Loop;
}

namespace asyncurl
{
class handle;

/*********************************************************************************************************************/
class mhandle
{
public:

    using TCbSocket = std::function<int(void*,int64_t,int,void*,void*)>;
    using TCbTimer = std::function<int(void*,long,void*)>;
    using TCbPush = std::function<int(void*,void*,size_t, void*, void*)>;

    /*!
     * @brief MHDL_RetCode describes the return codes of the asyncurl::mhandle class methods
     */
    typedef enum {
        MHDL_OK = 0, /*!< OK */
        MHDL_ADD_OWNED, /*!< An handle who is owned by another mhandle was attempted to get added */
        MHDL_ADD_ALREADY, /*!< An handle already owned was attempted to get added again */
        MHDL_REMOVE_OWNED, /*!< An handle who is owned by another mhandle was attempted to get removed */
        MHDL_REMOVE_ALREADY, /*!< An handle already removed (or never added) was attempted to get removed again */
        MHDL_BAD_HANDLE, /*!< An handle passed-in is not a valid handle */
        MHDL_OUT_OF_MEM, /*!< An dynamic allocation call failed (you were probably too greedy) */
        MHDL_INTERNAL_ERROR /*!< Internal error */
    } MHDL_RetCode;

private:
    void* curl_multi__{ nullptr }; /*!< raw curl multi-handle (CURL::CURLM) */

    TCbSocket cb_socket__{};
    TCbTimer cb_timer__{};
    TCbPush cb_push__{};

    // créer std::unique_ptr<loop::Loop::Timeout> pour les timeouts
    // créer une std::map<,std::unique_ptr<loop::Loop::IO>>

    mhandle(const mhandle&) = delete;
    mhandle& operator=(const mhandle&) = delete;
    mhandle(mhandle&&) = delete;
    mhandle& operator=(mhandle&&) = delete;

protected:
    MHDL_RetCode set_opt_long(int id, long val) noexcept;
    MHDL_RetCode set_opt_ptr(int id, const void* val) noexcept;
    MHDL_RetCode set_opt_bool(int id, bool val) noexcept;

public:
    mhandle(loop::Loop&);
    ~mhandle() noexcept;

    MHDL_RetCode add_handle(handle&) noexcept;
    MHDL_RetCode remove_handle(handle&) noexcept;

    //----- SHOULD PROBABLY BECOME PRIVATE -----//
    MHDL_RetCode set_pushfunction(TCbPush) noexcept;
    MHDL_RetCode set_pushdata(void*) noexcept;

    MHDL_RetCode set_socketfunction(TCbSocket) noexcept;
    MHDL_RetCode set_socketdata(void*) noexcept;

    MHDL_RetCode set_timerfunction(TCbTimer) noexcept;
    MHDL_RetCode set_timerdata(void*) noexcept;
    //------------------------------------------//

    //----- SHOULD PROBABLY BECOME SINGLE CALL -----//
    MHDL_RetCode set_max_concurrent_streams(long) noexcept;
    MHDL_RetCode set_max_host_connections(long) noexcept;
    MHDL_RetCode set_max_pipeline_length(long) noexcept;
    MHDL_RetCode set_max_total_connections(long) noexcept;
    MHDL_RetCode set_maxconnects(long) noexcept;
    MHDL_RetCode set_pipelining(long) noexcept;
    //----------------------------------------------//

    void* raw(void) noexcept;
};

} // namespace asyncurl

#endif // INCLUDE_ASYNCURL_MHANDLE_H
