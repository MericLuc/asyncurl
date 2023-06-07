/**
 * Copyright (C) 2023 Osmozis SA - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <asyncurl/handle.hpp>
#include <asyncurl/mhandle.hpp>

#include <Loop.h>
#include <curl/curl.h>

#include <map>
#include <stdexcept>

using namespace loop;

#define MHDL_STOPPED -1

namespace asyncurl
{
//---------------------------------------------------------------------------------------------------------------------
// STATIC FUNCTIONS
//---------------------------------------------------------------------------------------------------------------------

/**
 * @brief timer_callback - Callback called by curl when it is interested in setting a timer inside libevent.
 *
 * @param multi_handle The session concerned..
 * @param timeout_ms The timeout when curl wants to place (or -1 to delete it)
 * @param userp Private user data pointer
 * @return A retcode indicating libcurl how its request was treated.
 */
int
mhandle::timer_callback(void* /*multi_handle*/, long timeout_ms, void* userp)
{
    mhandle* This{ static_cast<mhandle*>(userp) };

    if (-1 == timeout_ms)
        This->timeout__->cancel();
    else
        This->timeout__->set(timeout_ms);

    return CURLM_OK;
}

/**
 * @brief socket_callback - Callback called by curl when it is interested in socket events
 *
 * @param easy The transfer concerned
 * @param s The socket of interest
 * @param what The event(s) of interest on the socket
 * @param clientp A private callback pointer
 * @param socketp A private socket pointer
 * @return A retcode indicating libcurl how its request was treated.
 */
int
mhandle::socket_callback(void* easy, size_t s, int what, void* clientp, void* socketp)
{
    mhandle*  This{ static_cast<mhandle*>(clientp) };
    Loop::IO* io{ static_cast<Loop::IO*>(socketp) };
    CURL*     hdl{ static_cast<CURL*>(easy) };

    if (CURL_POLL_REMOVE == what)
    {
        if (nullptr != io) io->setRequestedEvents(0);
        return CURLM_OK;
    }

    if (nullptr == io)
    {
        This->ios__[hdl] = std::make_unique<Loop::IO>(s, This->loop__);

        io = This->ios__[hdl].get();
        io->onEvent([This, io](int evt) {
            int evt_bitmask{ 0 };
            int rhandles{ This->running_handles__ };

            if (evt & Loop::IO::READ) evt_bitmask |= CURL_CSELECT_IN;
            if (evt & Loop::IO::WRITE) evt_bitmask |= CURL_CSELECT_OUT;

            if (auto ret =
                  curl_multi_socket_action(This->curl_multi__, io->getFd(), evt_bitmask, &This->running_handles__);
                CURLM_OK != ret)
            {
                This->handle_stop(ret);
                return;
            }
            if (This->running_handles__ != rhandles) This->handle_msgs();
        });

        curl_multi_assign(This->curl_multi__, s, io);
    }

    short int evts{ 0 };
    switch (what)
    {
        case CURL_POLL_INOUT: evts |= (Loop::IO::READ | Loop::IO::WRITE); break;
        case CURL_POLL_IN: evts |= Loop::IO::READ; break;
        case CURL_POLL_OUT: evts |= Loop::IO::WRITE; break;
        default: break;
    }

    io->setFd(s);
    io->setRequestedEvents(evts);

    return CURLM_OK;
}

//---------------------------------------------------------------------------------------------------------------------
// CONSTRUCTORS/DESTRUCTOR
//---------------------------------------------------------------------------------------------------------------------

/**
 * @brief mhandle - Constructor
 * @param loop The loop that will be used by the session to drive its transfer(s).
 *
 * @warning The loop should outlive the session
 * @warning The loop should not be accessed in any other thread
 */
mhandle::mhandle(loop::Loop& loop)
  : curl_multi__{ curl_multi_init() }
  , loop__{ loop }
  , timeout__{ std::make_unique<Loop::Timeout>(loop__) }
{
    if (nullptr == curl_multi__) throw std::runtime_error("Unable to create underlying stack");

    // Setup timer callback and data
    timeout__->onTimeout([this]() {
        int rhandles{ this->running_handles__ };

        if (auto ret = curl_multi_socket_action(this->curl_multi__, CURL_SOCKET_TIMEOUT, 0, &this->running_handles__);
            CURLM_OK != ret)
        {
            this->handle_stop(ret);
            return;
        }

        if (this->running_handles__ != rhandles) this->handle_msgs();
    });
    curl_multi_setopt(curl_multi__, CURLMOPT_TIMERDATA, this);
    curl_multi_setopt(curl_multi__, CURLMOPT_TIMERFUNCTION, timer_callback);

    // Setup socket callback and data
    curl_multi_setopt(curl_multi__, CURLMOPT_SOCKETDATA, this);
    curl_multi_setopt(curl_multi__, CURLMOPT_SOCKETFUNCTION, socket_callback);

    // TODO v2 - Setup push callback and data (HTTP/2)
}

/**
 * @brief ~mhandle - Destructor
 */
mhandle::~mhandle() noexcept
{
    handle_stop(CURLM_OK);
}

//---------------------------------------------------------------------------------------------------------------------
// HANDLERS INTERFACE
//---------------------------------------------------------------------------------------------------------------------

/**
 * @brief add_handle - Adds an handle (a transfer) to the multi session.
 *
 * By doing so, you give control of the transfer over the multi session.
 * The multi session controls a cache of connections that are shared between its transfers, so you can safely remove a
 * handler without losing connections.
 * @param h The handle to add
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @note If you want to add an handle from another session, you must first remove it from its previous session.
 */
mhandle::MHDL_RetCode
mhandle::add_handle(handle& h) noexcept
{
    if (MHDL_STOPPED == running_handles__) return MHDL_INTERNAL_ERROR;
    if (this == h.multi_handler__) return MHDL_ADD_ALREADY;
    if (nullptr != h.multi_handler__) return MHDL_ADD_OWNED;

    CURL* raw{ static_cast<CURL*>(h.raw()) };
    if (auto ret{ curl_multi_add_handle(curl_multi__, raw) }; CURLM_OK == ret)
    {
        h.multi_handler__ = this;
        handles__[raw]    = &h;

        // Start everything if needed (first handler added)
        if (0 == running_handles__)
        {
            if (auto ret = curl_multi_socket_action(curl_multi__, CURL_SOCKET_TIMEOUT, 0, &running_handles__);
                CURLM_OK != ret)
            {
                this->handle_stop(ret);
                return MHDL_INTERNAL_ERROR;
            }
            this->handle_msgs();
        }
        return MHDL_OK;
    }

    return MHDL_INTERNAL_ERROR;
}

/**
 * @brief remove_handle - Removes a given handle (a transfer) from the multi_handle.
 *
 * This will remove the specified handle from this multi session control.
 * After removal, it is perfectly legal to reuse the handle (e.g. by assigning it to another multi_handle)
 * @param h The handle to remove
 * @return A return code described by the \a MHDL_RetCode enumerate
 */
mhandle::MHDL_RetCode
mhandle::remove_handle(handle& h) noexcept
{
    if (nullptr == h.multi_handler__) return MHDL_REMOVE_ALREADY;
    if (this != h.multi_handler__) return MHDL_REMOVE_OWNED;

    CURL* raw{ static_cast<CURL*>(h.raw()) };
    if (auto ret{ curl_multi_remove_handle(curl_multi__, raw) }; CURLM_OK == ret)
    {
        h.multi_handler__ = nullptr;
        if (std::end(handles__) != handles__.find(raw)) handles__.erase(raw);
        if (std::end(ios__) != ios__.find(raw)) ios__.erase(raw);

        return MHDL_OK;
    }

    return MHDL_INTERNAL_ERROR;
}

/**
 * @brief raw get the raw curl multi-handle (CURLM::handle)
 *
 * @warning You should not be using this, unless you absolutely need to use curl features that are not provided by
 * the \a asyncurl library.
 * @return then raw multi-handle
 */
void*
mhandle::raw(void) noexcept
{
    return curl_multi__;
}

//---------------------------------------------------------------------------------------------------------------------
// OPTIONS
// Change specific multi handle options - allowing to control the way it will behave.
// \see https://curl.se/libcurl/c/curl_multi_setopt.html
//---------------------------------------------------------------------------------------------------------------------

/**
 * @brief set_opt_long - Set an information element (\see CURLMOPT type) of type long
 *
 * @param id The information's id to set
 * @param val The value to set
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/curl_multi_setopt.html
 */
mhandle::MHDL_RetCode
mhandle::set_opt_long(int id, long val) noexcept
{
    return (CURLM_OK == curl_multi_setopt(curl_multi__, static_cast<CURLMoption>(id), val)) ? MHDL_OK
                                                                                            : MHDL_INTERNAL_ERROR;
}

/**
 * @brief set_opt_ptr - Set an information element (\see CURLMOPT type) of type ptr
 *
 * @param id The information's id to set
 * @param val The value to set
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/curl_multi_setopt.html
 */
mhandle::MHDL_RetCode
mhandle::set_opt_ptr(int id, const void* val) noexcept
{
    return (CURLM_OK == curl_multi_setopt(curl_multi__, static_cast<CURLMoption>(id), val)) ? MHDL_OK
                                                                                            : MHDL_INTERNAL_ERROR;
}

/**
 * @brief set_opt_bool - Set an information element (\see CURLMOPT type) of type bool
 *
 * @param id The information's id to set
 * @param val The value to set
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/curl_multi_setopt.html
 */
mhandle::MHDL_RetCode
mhandle::set_opt_bool(int id, bool val) noexcept
{
    return (CURLM_OK == curl_multi_setopt(curl_multi__, static_cast<CURLMoption>(id), static_cast<long>(val ? 1 : 0)))
             ? MHDL_OK
             : MHDL_INTERNAL_ERROR;
}

/**
 * @brief set_opt_offset - Set an information element of type offset
 *
 * @param id The information's id to set
 * @param val The value to set
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/curl_multi_setopt.html
 */
mhandle::MHDL_RetCode
mhandle::set_opt_offset(int id, long val) noexcept
{
    if (CURLOPTTYPE_OFF_T != (id / 10000) * 10000) return MHDL_BAD_PARAM;
    return (CURLM_OK == curl_multi_setopt(curl_multi__, static_cast<CURLMoption>(id), val)) ? MHDL_OK
                                                                                            : MHDL_INTERNAL_ERROR;
}

/**
 * @brief set_opt - Set an option modifying the behaviour of the session
 *
 * @param id The identifier of the option to set
 * @param val The value to set
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/curl_multi_setopt.html
 */
mhandle::MHDL_RetCode
mhandle::set_opt(int id, std::any val) noexcept
{
    MHDL_RetCode ret{ MHDL_BAD_PARAM };
    const auto   curType{ (id / 10000) * 10000 };

    if (!val.has_value()) return ret;

    if (typeid(long) == val.type() && (CURLOPTTYPE_LONG == curType))
    {
        ret = set_opt_long(id, std::any_cast<long>(val));
    }
    else if (typeid(long) == val.type() && (CURLOPTTYPE_OFF_T == curType))
    {
        ret = set_opt_offset(id, std::any_cast<long>(val));
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

/**
 * @brief set_max_concurrent_streams - Set the maximum number of concurrent stream (HTTP/2)
 *
 * @param max The maximum number of concurrent streams for connection done using HTTP/2
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @note This is a convenience function - it could be performed using the generic method \see mhandle::set_opt()
 * @see https://curl.se/libcurl/c/CURLMOPT_MAX_CONCURRENT_STREAMS.html
 */
mhandle::MHDL_RetCode
mhandle::set_max_concurrent_streams(long max) noexcept
{
    return set_opt_long(CURLMOPT_MAX_CONCURRENT_STREAMS, max);
}

/**
 * @brief set_max_host_connections - Set the maximum conncetions to host
 *
 * @param max The maximum amount of simultaneously open connections to a single host (hostname + port)
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @note This is a convenience function - it could be performed using the generic method \see mhandle::set_opt()
 * @see https://curl.se/libcurl/c/CURLMOPT_MAX_HOST_CONNECTIONS.html
 */
mhandle::MHDL_RetCode
mhandle::set_max_host_connections(long max) noexcept
{
    return set_opt_long(CURLMOPT_MAX_HOST_CONNECTIONS, max);
}

/**
 * @brief set_max_pipeline_length - Set the maximum number of requests in a HTTP/1.1 pipeline.
 *
 * When reaching this limit, another connection to the same host will be used.
 * @param max The maximum amount of simultaneously open connections in a HTTP/1.1 pipeline
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @note This is a convenience function - it could be performed using the generic method \see mhandle::set_opt()
 * @see https://curl.se/libcurl/c/CURLMOPT_MAX_PIPELINE_LENGTH.html
 */
mhandle::MHDL_RetCode
mhandle::set_max_pipeline_length(long max) noexcept
{
    return set_opt_long(CURLMOPT_MAX_PIPELINE_LENGTH, max);
}

/**
 * @brief set_max_pipeline_length - Set the maximum number of simultaneously open connections
 *
 * When reaching the limit, the sessions will be pending until there are available connections
 * @param max The maximum amount of simultaneously open connections
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @note This is a convenience function - it could be performed using the generic method \see mhandle::set_opt()
 * @see https://curl.se/libcurl/c/CURLMOPT_MAX_TOTAL_CONNECTIONS.html
 */
mhandle::MHDL_RetCode
mhandle::set_max_total_connections(long max) noexcept
{
    return set_opt_long(CURLMOPT_MAX_TOTAL_CONNECTIONS, max);
}

/**
 * @brief set_maxconnects - Set the size of the connections cache
 *
 * Setting this limit can prevent the cache from growing too much
 * @param max The maximum amount of cached connections
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @note This is a convenience function - it could be performed using the generic method \see mhandle::set_opt()
 * @see https://curl.se/libcurl/c/CURLMOPT_MAXCONNECTS.html
 */
mhandle::MHDL_RetCode
mhandle::set_maxconnects(long max) noexcept
{
    return set_opt_long(CURLMOPT_MAXCONNECTS, max);
}

/**
 * @brief set_pipelining - Enable/disable HTTP pipelining and multiplexing
 *
 * @param mask Enable HTTP pipelining and/or HTTP/2 multiplexing for this multi handle.
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @note This is a convenience function - it could be performed using the generic method \see mhandle::set_opt()
 * @see https://curl.se/libcurl/c/CURLMOPT_PIPELINING.html
 * @see CURLPIPE_NOTHING | CURLPIPE_HTTP1 | CURLPIPE_MULTIPLEX
 */
mhandle::MHDL_RetCode
mhandle::set_pipelining(long mask) noexcept
{
    return set_opt_long(CURLMOPT_PIPELINING, mask);
}

//---------------------------------------------------------------------------------------------------------------------
// CALLBACKS
// The sessions are event-driven (by miniloop) and need to setup callbacks to miniloop in order to work properly
// \see https://curl.se/libcurl/c/libcurl-multi.html
//---------------------------------------------------------------------------------------------------------------------

/**
 * @brief set_cb_error - Set error callback
 *
 * @param cb The callback used when the multi-session fails
 */
void
mhandle::set_cb_error(TCbError& cb) noexcept
{
    cb_error__ = cb;
}

/**
 * @brief mhandle::handle_stop - Manage the end of the session
 *
 * This will remove any individual transfer managed by the session (and call their done callback if any).
 * It will then cleanup its internal structures and call its error callback if any.
 * @param errCode The code associated to the stop (e.g. CURLM_OK for a 'normal' stop or another code in case of error)
 */
void
mhandle::handle_stop(int errCode) noexcept
{
    running_handles__ = MHDL_STOPPED;

    for (auto it{ std::cbegin(handles__) }; std::cend(handles__) != it;)
    {
        auto h{ it->second };

        it                 = handles__.erase(it);
        h->multi_handler__ = nullptr;

        h->cb_done__(handle::HDL_MULTI_STOPPED);
    }

    timeout__->cancel();

    curl_multi_cleanup(curl_multi__);

    if (CURLM_OK != errCode) cb_error__(errCode);
}

/**
 * @brief handle_msgs - Processes messages related to transfers
 *
 * Mostly used to inform transfers when they are done :)
 *
 * @see https://curl.se/libcurl/c/curl_multi_info_read.html
 */
void
mhandle::handle_msgs(void) noexcept
{
    int      dumb;
    CURLMsg* msg{ nullptr };

    while ((msg = curl_multi_info_read(this->curl_multi__, &dumb)))
    {
        if (CURLMSG_DONE != msg->msg) continue;

        CURL* hdl{ msg->easy_handle };
        auto  it{ handles__.find(hdl) };

        if (std::end(handles__) == it) continue;

        handle* h{ it->second };
        remove_handle(*h);
        if (h->cb_done__) h->cb_done__(msg->data.result);
    }
}

/**
 * @brief retCode2Str - Gives a human readable string for each retcodes
 *
 * @param rc The retcode
 * @return A human-readable representation of the retcode meaning
 */
std::string_view
mhandle::retCode2Str(mhandle::MHDL_RetCode rc) noexcept
{
    static const std::map<MHDL_RetCode, std::string> _retcodeMap{
        { MHDL_OK, "ok" },
        { MHDL_BAD_PARAM, "bad parameter" },
        { MHDL_ADD_OWNED, "handle already owned by another session" },
        { MHDL_ADD_ALREADY, "handle already owned by this session" },
        { MHDL_REMOVE_OWNED, "handle already owned by another session" },
        { MHDL_REMOVE_ALREADY, "handle not owned by this session" },
        { MHDL_BAD_HANDLE, "invalid handle" },
        { MHDL_OUT_OF_MEM, "out of memory" },
        { MHDL_INTERNAL_ERROR, "internal error" }
    };

    return _retcodeMap.at(rc);
}

} // namespace asyncurl
