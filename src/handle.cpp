/**
 * Copyright (C) 2023 Osmozis SA - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <asyncurl/handle.hpp>
#include <asyncurl/list.hpp>
#include <asyncurl/mhandle.hpp>
#include <curl/curl.h>

#include <map>
#include <stdexcept>

namespace asyncurl
{
typedef size_t (*CURL_WRITEFUNCTION_PTR)(char*, size_t, size_t, void*);
typedef size_t (*CURL_READFUNCTION_PTR)(char*, size_t, size_t, void*);
typedef int (*CURL_PROGRESSFUNCTION_PTR)(void*, curl_off_t, curl_off_t, curl_off_t, curl_off_t);
typedef size_t (*CURL_BUFFERFUNCTION_PTR)(char*, size_t, size_t, void*);
typedef int (*CURL_DEBUGFUNCTION_PTR)(CURL*, curl_infotype, char*, size_t, void*);

//---------------------------------------------------------------------------------------------------------------------
// CONSTRUCTORS/DESTRUCTOR
//---------------------------------------------------------------------------------------------------------------------

handle::handle(void* curl)
{
    curl_handle__ = static_cast<CURL*>(curl);

    if (nullptr == curl_handle__) throw std::runtime_error("Unable to creat a session handle");

    set_opt_ptr(CURLOPT_PRIVATE, this);
    set_opt_bool(CURLOPT_NOSIGNAL, true);

    set_cb_write([](char*, size_t sz) -> size_t { return sz; });
}

/**
 * @brief default constructor
 */
handle::handle()
  : curl_handle__{ curl_easy_init() }
{
    if (nullptr == curl_handle__) throw std::runtime_error("Unable to creat a session handle");

    set_opt_ptr(CURLOPT_PRIVATE, this);
    set_opt_bool(CURLOPT_NOSIGNAL, true); // multi-threaded applications

    // Setup to do nothing, otherwise data will be outputed to standard output stream
    set_cb_write([](char*, size_t sz) -> size_t { return sz; });
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
 * @brief handle::copy - Perform a copy of internal data
 *
 * Allows to avoid repeating series of set_opt()...
 * @return A new handle that you are responsible for
 *
 * @note You will still need to setup the required callbacks yourself
 */
handle*
handle::copy() noexcept
{
    handle* ret{ new handle(curl_easy_duphandle(curl_handle__)) };

    for (const auto& [id, l] : lists__)
        ret->set_opt_list(id, l);

    for (const auto& [id, str] : strings__)
        ret->set_opt_string(id, str.c_str());

    return ret;
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
 * @brief pause - Pause the transfer in one or both directions
 *
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
 * @brief is_paused - Test if the transfer is paused in one or both directions
 *
 * @param bitmask CURL pause mask (CURLPAUSE_RECV, CURLPAUSE_SEND, CURLPAUSE_ALL, CURLPAUSE_CONT)
 * @return true if the transfer is paused in the required direction(s), false otherwise
 */
bool
handle::is_paused(int bitmask) noexcept
{
    return (0 != (flags__ & bitmask));
}

/**
 * @brief unpause - Unpause the transfer in one or both directions
 *
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
 * @brief get_info_list - Get an information element of type list
 * @param id The information's id to get
 * @param val The variable to stock the result to
 * @return A return code described by the \a HDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/curl_easy_getinfo.html
 */
handle::HDL_RetCode
handle::get_info_list(int id, list& val) const noexcept
{
    struct curl_slist* v{ nullptr };
    if (CURLINFO_SLIST != (id & CURLINFO_TYPEMASK)) return HDL_BAD_PARAM;

    auto ret{ curl_easy_getinfo(curl_handle__, static_cast<CURLINFO>(id), &v) };
    if (CURLE_OK == ret)
    {
        val = list(v, list::owns);
        return HDL_OK;
    }
    return HDL_INTERNAL_ERROR;
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

    strings__[id] = val;
    return CURLE_OK == curl_easy_setopt(curl_handle__, static_cast<CURLoption>(id), strings__[id].c_str())
             ? HDL_OK
             : HDL_INTERNAL_ERROR;
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
 * @brief Set an information element of type list
 * @param id The information's id to set
 * @param val The value to set
 * @return A return code described by the \a HDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/curl_easy_setopt.html
 */
handle::HDL_RetCode
handle::set_opt_list(int id, const list& val) noexcept
{
    lists__[id] = val;
    if (CURLOPTTYPE_SLISTPOINT != (id / 10000) * 10000) return HDL_BAD_PARAM;

    auto ret{ curl_easy_setopt(curl_handle__, static_cast<CURLoption>(id), lists__[id].head__) };

    if (CURLE_OK != ret)
    {
        lists__.erase(id);
        return HDL_INTERNAL_ERROR;
    }

    return HDL_OK;
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
        case CURLINFO_SLIST: {
            list val;
            if (ret.ret = get_info_list(id, val); HDL_OK == ret.ret) ret.value = val;
        }
        break;
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

    if (typeid(list) == val.type() && (CURLOPTTYPE_SLISTPOINT == curType))
    {
        ret = set_opt_list(id, std::any_cast<list>(val));
    }
    else if (typeid(long) == val.type() && (CURLOPTTYPE_LONG == curType))
    {
        ret = set_opt_long(id, std::any_cast<long>(val));
    }
    else if (typeid(long) == val.type() && (CURLOPTTYPE_OFF_T == curType))
    {
        ret = set_opt_offset(id, std::any_cast<long>(val));
    }
    else if (typeid(std::string) == val.type() && (CURLOPTTYPE_STRINGPOINT == curType))
    {
        ret = set_opt_string(id, std::any_cast<const std::string&>(val).c_str());
    }
    else if (typeid(bool) == val.type() && (CURLOPTTYPE_LONG == curType))
    {
        ret = set_opt_bool(id, std::any_cast<bool>(val));
    }
    else if (CURLOPTTYPE_OBJECTPOINT == curType)
    {
        try
        {
            auto s = std::any_cast<void*>(val);
            ret    = set_opt_ptr(id, s);
        }
        catch (const std::bad_any_cast& e)
        {}
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
    lists__.clear();
    strings__.clear();

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
handle::set_cb_write(const TCbWrite& cb) noexcept
{
    // See https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
    constexpr auto _hidden{ [](char*  ptr,   // delivered data
                               size_t size,  // always 1
                               size_t nmemb, // size of the data
                               void*  userdata) -> size_t {
        auto       This{ static_cast<handle*>(userdata) };
        size_t     ret{ 0 };

        if (nullptr == This) return 0;

        if (This->cb_write__) ret = This->cb_write__(ptr, size * nmemb);

        // This is a magic return code for the write callback that, when returned, will signal libcurl to pause
        // receiving on the current transfer.
        // You should not need to use this as you have handle::pause() methods at your disposal
        if (CURL_WRITEFUNC_PAUSE == ret) This->flags__ |= CURLPAUSE_RECV;

        return ret;
    } };

    cb_write__ = cb;

    auto ret{ curl_easy_setopt(curl_handle__, CURLOPT_WRITEFUNCTION, static_cast<CURL_WRITEFUNCTION_PTR>(_hidden)) };
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
handle::set_cb_read(const TCbRead& cb) noexcept
{
    // See https://curl.se/libcurl/c/CURLOPT_READFUNCTION.html
    constexpr auto _hidden{ [](char*  buffer, // delivered data
                               size_t size,   // always 1
                               size_t nitems, // size of the data
                               void*  userdata) -> size_t {
        auto       This{ static_cast<handle*>(userdata) };
        size_t     ret{ 0 };

        if (nullptr == This) return 0; // Abort the transfert

        if (This->cb_read__) ret = This->cb_read__(buffer, size * nitems);

        // This is a magic return code for the read callback that, when returned, will signal libcurl to pause
        // sending on the current transfer.
        // You should not need to use this as you have handle::pause() methods at your disposal
        if (CURL_READFUNC_PAUSE == ret) This->flags__ |= CURLPAUSE_SEND;

        return ret;
    } };

    cb_read__ = cb;

    auto ret{ curl_easy_setopt(curl_handle__, CURLOPT_READFUNCTION, static_cast<CURL_READFUNCTION_PTR>(_hidden)) };
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
handle::set_cb_progress(const TCbProgress& cb) noexcept
{
    // See https://curl.se/libcurl/c/CURLOPT_XFERINFOFUNCTION.html
    constexpr auto _hidden{ [](void*      clientp, // pointer set with CURLOPT_XFERINFODATA
                               curl_off_t dltotal, //  total number of bytes expects to download in this transfer
                               curl_off_t dlnow,   // number of bytes downloaded so far
                               curl_off_t ultotal, //  total number of bytes expected to upload in this transfer
                               curl_off_t ulnow    // number of bytes uploaded so far
                               ) -> int {
        auto       This{ static_cast<handle*>(clientp) };
        int        ret{ 0 };

        if (nullptr == This) return 0; // Abort the transfert

        if (This->cb_progress__) ret = This->cb_progress__(dltotal, dlnow, ultotal, ulnow);
        return ret;
    } };

    cb_progress__ = cb;

    auto ret{
        curl_easy_setopt(curl_handle__, CURLOPT_XFERINFOFUNCTION, static_cast<CURL_PROGRESSFUNCTION_PTR>(_hidden))
    };
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
handle::set_cb_header(const TCbHeader& cb) noexcept
{
    // See https://curl.se/libcurl/c/CURLOPT_HEADERFUNCTION.html
    constexpr auto _hidden{ [](char*  buffer,  // points to the delivered data
                               size_t size,    // always 1
                               size_t nitems,  // nsize of the data
                               void*  userdata // pointer set with the CURLOPT_HEADERDATA option
                               ) -> size_t {
        auto       This{ static_cast<handle*>(userdata) };
        size_t     ret{ 0 };

        if (nullptr == This) return 0; // Abort the transfert

        if (This->cb_header__) ret = This->cb_header__(buffer, size * nitems);

        return ret;
    } };

    cb_header__ = cb;

    auto ret{ curl_easy_setopt(curl_handle__, CURLOPT_HEADERFUNCTION, static_cast<CURL_BUFFERFUNCTION_PTR>(_hidden)) };
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
handle::set_cb_debug(const TCbDebug& cb) noexcept
{
    // See https://curl.se/libcurl/c/CURLOPT_DEBUGFUNCTION.html
    constexpr auto _hidden{ [](CURL*         hndl,   // the easy handle of the transfer
                               curl_infotype type,   // the kind of data to treat
                               char*         data,   // the buffer pointing to the data (NOT NULL TERMINATED)
                               size_t        size,   // the exact size of the data
                               void*         clientp // pointer set with the CURLOPT_DEBUGDATA option
                               ) -> int {
        auto       This{ static_cast<handle*>(clientp) };
        int        ret{ 0 };

        if (nullptr == This) return 0; // Nothing happens

        if (This->cb_debug__) ret = This->cb_debug__(hndl, type, data, size, clientp);

        return ret;
    } };

    cb_debug__ = cb;

    auto ret{ curl_easy_setopt(curl_handle__, CURLOPT_DEBUGFUNCTION, static_cast<CURL_DEBUGFUNCTION_PTR>(_hidden)) };
    if (CURLE_OK == ret) set_opt_ptr(CURLOPT_DEBUGDATA, this);

    return (CURLE_OK == ret) ? HDL_OK : HDL_INTERNAL_ERROR;
}

/**
 * @brief set_cb_done - Set the done callback
 *
 * @param cb The callback called when the transfer is done.
 * @return A return code described by the \a HDL_RetCode enumerate
 *
 * @warning In asynchronous mode, when you enter this callback, the handle has been released by the session.
 * So you can immediately ask for a new transfer within the callback by calling mhandle::add_handle() again.
 * @warning In asynchronous mode, if the code is MDL_RetCode::HDL_MULTI_STOPPED, you can not use the session again.
 */
handle::HDL_RetCode
handle::set_cb_done(const TCbDone& cb) noexcept
{
    cb_done__ = cb;
    return HDL_OK;
}

/**
 * @brief retCode2Str - Gives a human readable string for each retcodes
 *
 * @param rc The retcode
 * @return A human-readable representation of the retcode meaning
 */
std::string_view
handle::retCode2Str(handle::HDL_RetCode rc) noexcept
{
    static const std::map<HDL_RetCode, std::string> _retcodeMap{ { HDL_MULTI_STOPPED, "multi-session stopped" },
                                                                 { HDL_OK, "ok" },
                                                                 { HDL_BAD_PARAM, "bad parameter" },
                                                                 { HDL_BAD_FUNCTION, "bad function call" },
                                                                 { HDL_OUT_OF_MEM, "out of memory" },
                                                                 { HDL_INTERNAL_ERROR, "internal error" } };

    return _retcodeMap.at(rc);
}

} // namespace asyncurl
