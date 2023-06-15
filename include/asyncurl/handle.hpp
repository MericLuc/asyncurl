/**
 * Copyright (C) 2023 Osmozis SA - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file handle.hpp
 * @brief Wrapper around curl easy handle
 * \see https://everything.curl.dev/libcurl/easyhandle for more informations
 *
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

#include <any>
#include <cstddef>    // size_t
#include <cstdint>    // int64_t
#include <functional> // std::function
#include <map>
#include <string>
#include <string_view>

namespace asyncurl
{
class mhandle;
class list;

/*********************************************************************************************************************/
class handle
{
    friend class mhandle;

public:
    using TCbWrite    = std::function<size_t(char*, size_t)>;
    using TCbRead     = std::function<size_t(char*, size_t)>;
    using TCbProgress = std::function<int(int64_t, int64_t, int64_t, int64_t)>;
    using TCbHeader   = std::function<size_t(char*, size_t)>;
    using TCbDebug    = std::function<int(void*, int, char*, size_t, void*)>;
    using TCbDone     = std::function<void(int)>;

    /**
     * @brief HDL_RetCode describes the return codes of the asyncurl::handle class methods
     */
    typedef enum
    {
        HDL_MULTI_STOPPED = -1, /*!< In case the handle is associated to a multi session, the end of the session */
        HDL_OK            = 0,  /*!< OK */
        HDL_BAD_PARAM,          /*!< An invalid parameter was passed to a function */
        HDL_BAD_FUNCTION,       /*!< A function has been called when it should not be */
        HDL_OUT_OF_MEM,         /*!< An dynamic allocation call failed (you were probably too greedy) */
        HDL_INTERNAL_ERROR      /*!< Internal error */
    } HDL_RetCode;

    /**
     * @brief The handle_info structure contains the answer to asyncurl::handle queries
     * - an information request (\see mhandle::get_info)
     * - on option request (\see mhandle::set_opt)
     */
    struct handle_ret
    {
        HDL_RetCode ret;   /*!< The return code (indicates whether it succeeded or not) */
        std::any    value; /*!< The value (if any) of the required information */
    };

private:
    mhandle*                   multi_handler__{ nullptr };
    void*                      curl_handle__{ nullptr }; /*< CURL easy handle - you do NOT want to mess with this */
    int                        flags__{ 0 };
    std::map<int, list>        lists__;
    std::map<int, std::string> strings__;

    TCbWrite    cb_write__{ nullptr };
    TCbRead     cb_read__{ nullptr };
    TCbProgress cb_progress__{ nullptr };
    TCbHeader   cb_header__{ nullptr };
    TCbDebug    cb_debug__{ nullptr };
    TCbDone     cb_done__{ nullptr };

    handle(const handle&) = delete;
    handle& operator=(const handle&) = delete;
    handle(handle&&)                 = delete;
    handle& operator=(handle&&) = delete;

    handle(void*);

protected:
    HDL_RetCode get_info_long(int, long&) const noexcept;
    HDL_RetCode get_info_socket(int, uint64_t&) const noexcept;
    HDL_RetCode get_info_double(int, double&) const noexcept;
    HDL_RetCode get_info_string(int, std::string&) const noexcept;
    HDL_RetCode get_info_list(int id, list& val) const noexcept;

    HDL_RetCode set_opt_long(int id, long val) noexcept;
    HDL_RetCode set_opt_offset(int id, long val) noexcept;
    HDL_RetCode set_opt_ptr(int id, const void* val) noexcept;
    HDL_RetCode set_opt_string(int id, const char* val) noexcept;
    HDL_RetCode set_opt_bool(int id, bool val) noexcept;
    HDL_RetCode set_opt_list(int id, const list& val) noexcept;

public:
    handle();
    ~handle() noexcept;

    handle* copy() noexcept;

    void* raw(void) noexcept;

    HDL_RetCode set_cb_write(const TCbWrite&) noexcept;
    HDL_RetCode set_cb_read(const TCbRead&) noexcept;
    HDL_RetCode set_cb_progress(const TCbProgress&) noexcept;
    HDL_RetCode set_cb_header(const TCbHeader&) noexcept;
    HDL_RetCode set_cb_debug(const TCbDebug&) noexcept;
    HDL_RetCode set_cb_done(const TCbDone&) noexcept;

    HDL_RetCode perform_blocking(void) noexcept;
    void        reset(void) noexcept;

    bool pause(int bitmask) noexcept;
    bool unpause(int bitmask) noexcept;
    bool is_paused(int bitmask) noexcept;

    handle_ret  get_info(int id) noexcept;
    HDL_RetCode set_opt(int id, std::any val) noexcept;

    static std::string_view retCode2Str(HDL_RetCode) noexcept;
};

} // namespace asyncurl

#endif // INCLUDE_ASYNCURL_HANDLE_H
