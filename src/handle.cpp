/**
 * Copyright (C) 2023 Osmozis SA - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <asyncurl/handle.hpp>
#include <asyncurl/mhandle.hpp>
#include <curl/curl.h>

#include <stdexcept>

namespace asyncurl
{
//---------------------------------------------------------------------------------------------------------------------
// CONSTRUCTORS/DESTRUCTOR
//---------------------------------------------------------------------------------------------------------------------

/**
 * @brief default constructor
 */
handle::handle()
  : curl_handle__{ curl_easy_init() }
{
    if (nullptr == curl_handle__) throw std::runtime_error("Unable to creat a session handle");

    set_opt_ptr(CURLOPT_PRIVATE, this);
    set_opt_bool(CURLOPT_NOSIGNAL, true); // multi-threaded applications
}

/**
 * @brief destructor
 * Performs RAII cleaning
 * - Remove from its \a mhandle if necessary
 * - Clean the handle data, connections
 *
 * @note You should avoid (as much as possible) to destroy handles.
 * Reusability of handlers is the key for high performances. :)
 */
handle::~handle() noexcept
{
    if (nullptr != multi_handler__) multi_handler__->remove_handle(*this);
    if (nullptr != curl_handle__) curl_easy_cleanup(curl_handle__);
}

/**
 * @brief raw get the raw curl easy-handle (CURL::handle)
 * @warning You should not be using this, unless you absolutely need to use curl features that are not provided by
 * the \a asyncurl library.
 * @return then raw handle
 */
void*
handle::raw(void) noexcept
{
    return curl_handle__;
}

//---------------------------------------------------------------------------------------------------------------------
// UN/PAUSE TRANSFER
// \see https://curl.se/libcurl/c/curl_easy_pause.html
//---------------------------------------------------------------------------------------------------------------------

/**
 * @brief pause pause the transfer in one or both directions
 * @param bitmask CURL pause mask (CURLPAUSE_RECV, CURLPAUSE_SEND, CURLPAUSE_ALL, CURLPAUSE_CONT)
 * @return true if the command was successfull, false otherwise
 */
bool
handle::pause(int bitmask) noexcept
{
    const auto prev{ flags__ };
    flags__ |= (bitmask & CURLPAUSE_ALL);
    return (flags__ == prev) || (CURLE_OK == curl_easy_pause(curl_handle__, flags__ & CURLPAUSE_ALL));
}

/**
 * @brief is_paused test if the transfer is paused in one or both directions
 * @param bitmask CURL pause mask (CURLPAUSE_RECV, CURLPAUSE_SEND, CURLPAUSE_ALL, CURLPAUSE_CONT)
 * @return true if the transfer is paused in the required direction(s), false otherwise
 */
bool
handle::is_paused(int bitmask) noexcept
{
    return (0 != (flags__ & bitmask));
}

/**
 * @brief unpause unpause the transfer in one or both directions
 * @param bitmask CURL pause mask (CURLPAUSE_RECV, CURLPAUSE_SEND, CURLPAUSE_ALL, CURLPAUSE_CONT)
 * @return true if the command was successfull, false otherwise
 */
bool
handle::unpause(int bitmask) noexcept
{
    const auto old{ flags__ };
    flags__ &= ~(bitmask & CURLPAUSE_ALL);
    return (flags__ == old) || (CURLE_OK == curl_easy_pause(curl_handle__, flags__ & CURLPAUSE_ALL));
}

//---------------------------------------------------------------------------------------------------------------------
// OPTIONS
// Change specific socket options before the underlying socket is connected
// \see https://everything.curl.dev/libcurl/callbacks/sockopt
//---------------------------------------------------------------------------------------------------------------------

/**
 * @brief get_info_long - Get an information element of type long
 * @param id The information's id to get
 * @param val The variable to stock the result to
 * @return A return code described by the \a HDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/curl_easy_getinfo.html
 */
handle::HDL_RetCode
handle::get_info_long(int id, long& val) const noexcept
{
    if (CURLINFO_LONG != (id & CURLINFO_TYPEMASK)) return HDL_BAD_PARAM;
    return (CURLE_OK == curl_easy_getinfo(curl_handle__, static_cast<CURLINFO>(id), &val)) ? HDL_OK
                                                                                           : HDL_INTERNAL_ERROR;
}

/**
 * @brief get_info_socket - Get an information element of type long
 * @param id The information's id to get
 * @param val The variable to stock the result to
 * @return A return code described by the \a HDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/curl_easy_getinfo.html
 */
handle::HDL_RetCode
handle::get_info_socket(int id, uint64_t& val) const noexcept
{
    if (CURLINFO_SOCKET != (id & CURLINFO_TYPEMASK)) return HDL_BAD_PARAM;
    return (CURLE_OK == curl_easy_getinfo(curl_handle__, static_cast<CURLINFO>(id), &val)) ? HDL_OK
                                                                                           : HDL_INTERNAL_ERROR;
}

/**
 * @brief get_info_double - Get an information element of type long
 * @param id The information's id to get
 * @param val The variable to stock the result to
 * @return A return code described by the \a HDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/curl_easy_getinfo.html
 */
handle::HDL_RetCode
handle::get_info_double(int id, double& val) const noexcept
{
    if (CURLINFO_DOUBLE != (id & CURLINFO_TYPEMASK)) return HDL_BAD_PARAM;
    return (CURLE_OK == curl_easy_getinfo(curl_handle__, static_cast<CURLINFO>(id), &val)) ? HDL_OK
                                                                                           : HDL_INTERNAL_ERROR;
}

/**
 * @brief get_info_string - Get an information element of type string
 * @param id The information's id to get
 * @param val The variable to stock the result to
 * @return A return code described by the \a HDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/curl_easy_getinfo.html
 */
handle::HDL_RetCode
handle::get_info_string(int id, std::string& val) const noexcept
{
    if (CURLINFO_STRING != (id & CURLINFO_TYPEMASK)) return HDL_BAD_PARAM;
    return (CURLE_OK == curl_easy_getinfo(curl_handle__, static_cast<CURLINFO>(id), &val)) ? HDL_OK
                                                                                           : HDL_INTERNAL_ERROR;
}

/**
 * @brief Set an information element of type long
 * @param id The information's id to set
 * @param val The value to set
 * @return A return code described by the \a HDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/curl_easy_setopt.html
 */
handle::HDL_RetCode
handle::set_opt_long(int id, long val) noexcept
{
    if (CURLOPTTYPE_LONG != (id / 10000) * 10000) return HDL_BAD_PARAM;
    return CURLE_OK == curl_easy_setopt(curl_handle__, static_cast<CURLoption>(id), val) ? HDL_OK : HDL_INTERNAL_ERROR;
}

/**
 * @brief Set an information element of type offset
 * @param id The information's id to set
 * @param val The value to set
 * @return A return code described by the \a HDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/curl_easy_setopt.html
 */
handle::HDL_RetCode
handle::set_opt_offset(int id, long val) noexcept
{
    if (CURLOPTTYPE_OFF_T != (id / 10000) * 10000) return HDL_BAD_PARAM;
    return CURLE_OK == curl_easy_setopt(curl_handle__, static_cast<CURLoption>(id), val) ? HDL_OK : HDL_INTERNAL_ERROR;
}

/**
 * @brief Set an information element of type ptr
 * @param id The information's id to set
 * @param val The value to set
 * @return A return code described by the \a HDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/curl_easy_setopt.html
 */
handle::HDL_RetCode
handle::set_opt_ptr(int id, const void* val) noexcept
{
    if (CURLOPTTYPE_OBJECTPOINT != (id / 10000) * 10000) return HDL_BAD_PARAM;
    return CURLE_OK == curl_easy_setopt(curl_handle__, static_cast<CURLoption>(id), val) ? HDL_OK : HDL_INTERNAL_ERROR;
}

/**
 * @brief Set an information element of type string
 * @param id The information's id to set
 * @param val The value to set
 * @return A return code described by the \a HDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/curl_easy_setopt.html
 */
handle::HDL_RetCode
handle::set_opt_string(int id, const char* val) noexcept
{
    if (CURLOPTTYPE_STRINGPOINT != (id / 10000) * 10000) return HDL_BAD_PARAM;
    return CURLE_OK == curl_easy_setopt(curl_handle__, static_cast<CURLoption>(id), val) ? HDL_OK : HDL_INTERNAL_ERROR;
}

/**
 * @brief Set an information element of type bool
 * @param id The information's id to set
 * @param val The value to set
 * @return A return code described by the \a HDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/curl_easy_setopt.html
 */
handle::HDL_RetCode
handle::set_opt_bool(int id, bool val) noexcept
{
    return set_opt_long(id, static_cast<long>(val ? 1 : 0));
}

/**
 * @brief get_info - retrieve an information from the handle
 * @param id the identifier of the info to get (\see CURL::CURLINFO_ enumerate)
 * @return A structure containing the retcode of the operation aswell as the required value
 *
 * @see https://curl.se/libcurl/c/curl_easy_getinfo.html
 */
handle::handle_ret
handle::get_info(int id) noexcept
{
    handle_ret ret;

    switch (id & CURLINFO_TYPEMASK)
    {
        case CURLINFO_DOUBLE: {
            double val;
            if (ret.ret = get_info_double(id, val); HDL_OK == ret.ret) ret.value = val;
        }
        break;
        case CURLINFO_LONG: {
            long val;
            if (ret.ret = get_info_long(id, val); HDL_OK == ret.ret) ret.value = val;
        }
        break;
        case CURLINFO_STRING: {
            std::string val;
            if (ret.ret = get_info_string(id, val); HDL_OK == ret.ret) ret.value = val;
        }
        break;
        case CURLINFO_SOCKET: {
            uint64_t val;
            if (ret.ret = get_info_socket(id, val); HDL_OK == ret.ret) ret.value = val;
            break;
        }
        break;
        default: {
            ret.ret = HDL_BAD_PARAM;
        }
        break;
    }

    return ret;
}

/**
 * @brief set_opt - Set an option modifying the behaviour of the handle
 * @param id The identifier of the option to set
 * @param val The value to set the option to
 * @return A return code described by the \a HDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/curl_easy_setopt.html
 */
handle::HDL_RetCode
handle::set_opt(int id, std::any val) noexcept
{
    HDL_RetCode ret{ HDL_BAD_PARAM };
    const auto  curType{ (id / 10000) * 10000 };

    if (!val.has_value()) return ret;

    if (typeid(long) == val.type() && (CURLOPTTYPE_LONG == curType))
    {
        ret = set_opt_long(id, std::any_cast<long>(val));
    }
    else if (typeid(long) == val.type() && (CURLOPTTYPE_OFF_T == curType))
    {
        ret = set_opt_offset(id, std::any_cast<long>(val));
    }
    else if (typeid(std::string) == val.type() && (CURLOPTTYPE_STRINGPOINT == curType))
    {
        ret = set_opt_string(id, std::any_cast<std::string>(val).c_str());
    }
    else if (typeid(bool) == val.type() && (CURLOPTTYPE_LONG == curType))
    {
        ret = set_opt_bool(id, std::any_cast<bool>(val));
    }
    else if (CURLOPTTYPE_OBJECTPOINT == curType)
    {
        ret = set_opt_ptr(id, std::any_cast<void*>(val));
    }

    return ret;
}

//---------------------------------------------------------------------------------------------------------------------
// OPERATIONS
//---------------------------------------------------------------------------------------------------------------------

/**
 * @brief perform_blocking Perform a blocking transfer
 * You should first setup the handle hehaviour with handle::set_opt()
 * @return A return code described by the \a HDL_RetCode enumerate
 *
 * @note If you want to do many transfers, you are encouraged to use the same handle (connection reusage).
 * @see https://curl.se/libcurl/c/curl_easy_perform.html
 */
handle::HDL_RetCode
handle::perform_blocking(void) noexcept
{
    HDL_RetCode res{ HDL_BAD_FUNCTION };

    if (nullptr != multi_handler__) return res;

    res = (CURLE_OK == curl_easy_perform(curl_handle__)) ? HDL_OK : HDL_INTERNAL_ERROR;
    if (cb_done__) cb_done__(res);

    return res;
}

/**
 * @brief reset - Reinitializes all options of a session
 * @note It does not change connections, session ID cache, DNS cache, cookies...
 * @see https://curl.se/libcurl/c/curl_easy_reset.html
 */
void
handle::reset(void) noexcept
{
    if (nullptr != multi_handler__) multi_handler__->remove_handle(*this);

    curl_easy_reset(curl_handle__);
    set_opt_ptr(CURLOPT_PRIVATE, this);

    cb_write__    = {};
    cb_read__     = {};
    cb_progress__ = {};
    cb_header__   = {};
    cb_debug__    = {};
    cb_done__     = {};

    flags__ = 0;
}

//---------------------------------------------------------------------------------------------------------------------
// CALLBACKS
// \see https://everything.curl.dev/libcurl/callbacks
//---------------------------------------------------------------------------------------------------------------------

/**
 * @brief set_cb_write - set callback for writting received data
 * @param cb The callback called everytime a chunk of data has been read and needs to be processed (save).
 * @return A return code described by the \a HDL_RetCode enumerate
 */
handle::HDL_RetCode
handle::set_cb_write(TCbWrite cb) noexcept
{
    // See https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
    constexpr auto _hidden{ [](char*  ptr,   // delivered data
                               size_t size,  // always 1
                               size_t nmemb, // size of the data
                               void*  userdata) -> size_t {
        auto       This{ static_cast<handle*>(userdata) };

        if (nullptr == This) return 0;
        auto       ret{ This->cb_write__(ptr, size, nmemb, userdata) };

        // This is a magic return code for the write callback that, when returned, will signal libcurl to pause
        // receiving on the current transfer.
        // You should not need to use this as you have handle::pause() methods at your disposal
        if (CURL_WRITEFUNC_PAUSE == ret) This->flags__ |= CURLPAUSE_RECV;

        return ret;
    } };

    cb_write__ = cb;

    auto ret{ curl_easy_setopt(curl_handle__, CURLOPT_WRITEFUNCTION, _hidden) };
    if (CURLE_OK == ret) set_opt_ptr(CURLOPT_WRITEDATA, this);

    return (CURLE_OK == ret) ? HDL_OK : HDL_INTERNAL_ERROR;
}

/**
 * @brief set_cb_read - Set the read callback for data uploads
 * @param cb The callback called everytime the session needs to read data in order to send it to the peer.
 * e.g. In case of upload or POST requests.
 * @return A return code described by the \a HDL_RetCode enumerate
 */
handle::HDL_RetCode
handle::set_cb_read(TCbRead cb) noexcept
{
    // See https://curl.se/libcurl/c/CURLOPT_READFUNCTION.html
    constexpr auto _hidden{ [](char*  buffer, // delivered data
                               size_t size,   // always 1
                               size_t nitems, // size of the data
                               void*  userdata) -> size_t {
        auto       This{ static_cast<handle*>(userdata) };

        if (nullptr == This) return 0; // Abort the transfert

        auto       ret{ This->cb_read__(buffer, size, nitems, userdata) };

        // This is a magic return code for the read callback that, when returned, will signal libcurl to pause
        // sending on the current transfer.
        // You should not need to use this as you have handle::pause() methods at your disposal
        if (CURL_READFUNC_PAUSE == ret) This->flags__ |= CURLPAUSE_SEND;

        return ret;
    } };

    cb_read__ = cb;

    auto ret{ curl_easy_setopt(curl_handle__, CURLOPT_READFUNCTION, _hidden) };
    if (CURLE_OK == ret) set_opt_ptr(CURLOPT_READDATA, this);

    return (CURLE_OK == ret) ? HDL_OK : HDL_INTERNAL_ERROR;
}

/**
 * @brief set_cb_progress - Set the progress meter callback
 * @param cb The callback called everytime the session needs to read data in order to send it to the peer.
 * e.g. In case of upload or POST requests.
 * @return A return code described by the \a HDL_RetCode enumerate
 */
handle::HDL_RetCode
handle::set_cb_progress(TCbProgress cb) noexcept
{
    // See https://curl.se/libcurl/c/CURLOPT_XFERINFOFUNCTION.html
    constexpr auto _hidden{ [](void*      clientp, // pointer set with CURLOPT_XFERINFODATA
                               curl_off_t dltotal, //  total number of bytes expects to download in this transfer
                               curl_off_t dlnow,   // number of bytes downloaded so far
                               curl_off_t ultotal, //  total number of bytes expected to upload in this transfer
                               curl_off_t ulnow    // number of bytes uploaded so far
                               ) -> size_t {
        auto       This{ static_cast<handle*>(clientp) };

        if (nullptr == This) return 0; // Abort the transfert

        return This->cb_progress__(clientp, dltotal, dlnow, ultotal, ulnow);
    } };

    cb_progress__ = cb;

    auto ret{ curl_easy_setopt(curl_handle__, CURLOPT_READFUNCTION, _hidden) };
    if (CURLE_OK == ret)
    {
        set_opt_ptr(CURLOPT_XFERINFODATA, this);
        set_opt_bool(CURLOPT_NOPROGRESS, false);
    }

    return (CURLE_OK == ret) ? HDL_OK : HDL_INTERNAL_ERROR;
}

/**
 * @brief set_cb_header - Set the callback that receives header data
 * @param cb The callback called everytime the session receives a header data.
 * e.g. In case of upload or POST requests.
 * @return A return code described by the \a HDL_RetCode enumerate
 */
handle::HDL_RetCode
handle::set_cb_header(TCbHeader cb) noexcept
{
    // See https://curl.se/libcurl/c/CURLOPT_HEADERFUNCTION.html
    constexpr auto _hidden{ [](char*  buffer,  // points to the delivered data
                               size_t size,    // always 1
                               size_t nitems,  // nsize of the data
                               void*  userdata // pointer set with the CURLOPT_HEADERDATA option
                               ) -> size_t {
        auto       This{ static_cast<handle*>(userdata) };

        if (nullptr == This) return 0; // Abort the transfert

        return This->cb_header__(buffer, size, nitems, userdata);
    } };

    cb_header__ = cb;

    auto ret{ curl_easy_setopt(curl_handle__, CURLOPT_HEADERFUNCTION, _hidden) };
    if (CURLE_OK == ret) set_opt_ptr(CURLOPT_HEADERDATA, this);

    return (CURLE_OK == ret) ? HDL_OK : HDL_INTERNAL_ERROR;
}

/**
 * @brief set_cb_debug - Set the debug callback
 * @param cb The callback receiving debug informations.
 * @return A return code described by the \a HDL_RetCode enumerate
 *
 * @note The callback must return 0
 */
handle::HDL_RetCode
handle::set_cb_debug(TCbDebug cb) noexcept
{
    // See https://curl.se/libcurl/c/CURLOPT_DEBUGFUNCTION.html
    constexpr auto _hidden{ [](CURL*         hndl,   // the easy handle of the transfer
                               curl_infotype type,   // the kind of data to treat
                               char*         data,   // the buffer pointing to the data (NOT NULL TERMINATED)
                               size_t        size,   // the exact size of the data
                               void*         clientp // pointer set with the CURLOPT_DEBUGDATA option
                               ) -> int {
        auto       This{ static_cast<handle*>(clientp) };

        if (nullptr == This) return 0; // Nothing happens

        return This->cb_debug__(hndl, type, data, size, clientp);
    } };

    cb_debug__ = cb;

    auto ret{ curl_easy_setopt(curl_handle__, CURLOPT_DEBUGFUNCTION, _hidden) };
    if (CURLE_OK == ret) set_opt_ptr(CURLOPT_DEBUGDATA, this);

    return (CURLE_OK == ret) ? HDL_OK : HDL_INTERNAL_ERROR;
}

/**
 * @brief set_cb_done - Set the done callback
 * @param cb The callback called when the transfer is done.
 * @return A return code described by the \a HDL_RetCode enumerate
 *
 * @warning If the callback is called with a MDL_RetCode::HDL_MULTI_STOPPED, the handle will be removed from the
 * session. In that case, you will need to add it again before being able to reuse it.
 */
handle::HDL_RetCode
handle::set_cb_done(TCbDone cb) noexcept
{
    cb_done__ = cb;
    return HDL_OK;
}

} // namespace asyncurl
