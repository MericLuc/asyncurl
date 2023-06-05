/**
 * Copyright (C) 2023 Osmozis SA - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <asyncurl/handle.hpp>
#include <asyncurl/mhandle.hpp>

#include <Loop.h>
#include <curl/curl.h>

#include <stdexcept>

namespace asyncurl
{
//---------------------------------------------------------------------------------------------------------------------
// CONSTRUCTORS/DESTRUCTOR
//---------------------------------------------------------------------------------------------------------------------

mhandle::mhandle(loop::Loop& loop)
  : curl_multi__{ curl_multi_init() }
{
    // TODO - get loop
    if (nullptr == curl_multi__) throw std::runtime_error("Unable to create underlying stack");
}

mhandle::~mhandle() noexcept
{
    // TODO - cleanup sessions (we might want to keep those ?)
    curl_multi_cleanup(curl_multi__);
}

//---------------------------------------------------------------------------------------------------------------------
// ADD/REMOVE HANDLERS
//---------------------------------------------------------------------------------------------------------------------

/*!
 * @brief add_handle Adds an handle (a transfer) to the multi session.
 * By doing so, you give control of the transfer over the multi session.
 * The multi session controls a cache of connections that are shared between its transfers, so you can safely remove a
 * handler without losing connections.
 * @param h The handle to add
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * \note If you want to add an handle from another session, you must first remove it from its previous session.
 */
mhandle::MHDL_RetCode
mhandle::add_handle(handle& h) noexcept
{
    if (this == h.multi_handler__) return MHDL_ADD_ALREADY;
    if (nullptr != h.multi_handler__) return MHDL_ADD_OWNED;

    if (auto ret{ curl_multi_add_handle(curl_multi__, h.raw()) }; CURLM_OK == ret)
    {
        h.multi_handler__ = this;
        return MHDL_OK;
    }

    return MHDL_INTERNAL_ERROR;
}

/*!
 * @brief remove_handle Removes a given handle (a transfer) from the multi_handle.
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

    if (auto ret{ curl_multi_remove_handle(curl_multi__, h.raw()) }; CURLM_OK == ret)
    {
        h.multi_handler__ = nullptr;
        return MHDL_OK;
    }

    return MHDL_INTERNAL_ERROR;
}

/*!
 * @brief raw get the raw curl multi-handle (CURLM::handle)
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
 * @brief Set an information element (\see CURLMOPT type) of type long
 * @param id The information's id to set
 * @param val The value to set
 * @return A return code described by the \a MHDL_RetCode enumerate
 */
mhandle::MHDL_RetCode
mhandle::set_opt_long(int id, long val) noexcept
{
    return CURLM_OK == curl_multi_setopt(curl_multi__, static_cast<CURLMoption>(id), val) ? MHDL_OK
                                                                                          : MHDL_INTERNAL_ERROR;
}

/**
 * @brief Set an information element (\see CURLMOPT type) of type ptr
 * @param id The information's id to set
 * @param val The value to set
 * @return A return code described by the \a MHDL_RetCode enumerate
 */
mhandle::MHDL_RetCode
mhandle::set_opt_ptr(int id, const void* val) noexcept
{
    return CURLM_OK == curl_multi_setopt(curl_multi__, static_cast<CURLMoption>(id), val) ? MHDL_OK
                                                                                          : MHDL_INTERNAL_ERROR;
}

/**
 * @brief Set an information element (\see CURLMOPT type) of type bool
 * @param id The information's id to set
 * @param val The value to set
 * @return A return code described by the \a MHDL_RetCode enumerate
 */
mhandle::MHDL_RetCode
mhandle::set_opt_bool(int id, bool val) noexcept
{
    return CURLM_OK == curl_multi_setopt(curl_multi__, static_cast<CURLMoption>(id), static_cast<long>(val ? 1 : 0))
             ? MHDL_OK
             : MHDL_INTERNAL_ERROR;
}

/**
 * @brief set_socketfunction - Set the callback that will be used when updates occurs on a monitored socket.
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/CURLMOPT_SOCKETFUNCTION.html
 */
mhandle::MHDL_RetCode
mhandle::set_socketfunction(TCbSocket cb) noexcept
{
    // TODO
}

/**
 * @brief set_socketdata - Set the pointer to pass to the socket callback (\see set_socketfunction)
 * @param userdata Will be passed as the last argument to the socketfunction callback.
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/CURLMOPT_SOCKETDATA.html
 */
mhandle::MHDL_RetCode
mhandle::set_socketdata(void* userdata) noexcept
{
    return set_opt_ptr(CURLMOPT_SOCKETDATA, userdata);
}

/**
 * @brief set_timerfunction - Set the callback that will be used to receive timeout values.
 * It is used for example in timeouts, retries...
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/CURLMOPT_TIMERFUNCTION.html
 */
mhandle::MHDL_RetCode
mhandle::set_timerfunction(TCbTimer cb) noexcept
{
    // constexpr auto _hidden{ [](){} };
    // return set_opt_ptr(CURLMOPT_TIMERFUNCTION, cb);
}

/**
 * @brief set_timerdata - Set the pointer to pass to the timer callback (\see set_timerfunction)
 * @param userdata Will be passed as the last argument to the timerfunction callback.
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/CURLMOPT_TIMERDATA.html
 */
mhandle::MHDL_RetCode
mhandle::set_timerdata(void* userdata) noexcept
{
    return set_opt_ptr(CURLMOPT_TIMERDATA, userdata);
}

/**
 * @brief set_max_concurrent_streams - Set the maximum number of concurrent stream (HTTP/2)
 * @param max The maximum number of concurrent streams for connection done using HTTP/2
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/CURLMOPT_MAX_CONCURRENT_STREAMS.html
 */
mhandle::MHDL_RetCode
mhandle::set_max_concurrent_streams(long max) noexcept
{
    return set_opt_long(CURLMOPT_MAX_CONCURRENT_STREAMS, max);
}

/**
 * @brief set_max_host_connections - Set the maximum conncetions to host
 * @param max The maximum amount of simultaneously open connections to a single host (hostname + port)
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/CURLMOPT_MAX_HOST_CONNECTIONS.html
 */
mhandle::MHDL_RetCode
mhandle::set_max_host_connections(long max) noexcept
{
    return set_opt_long(CURLMOPT_MAX_HOST_CONNECTIONS, max);
}

/**
 * @brief set_max_pipeline_length - Set the maximum number of requests in a HTTP/1.1 pipeline.
 * When reaching this limit, another connection to the same host will be used.
 * @param max The maximum amount of simultaneously open connections in a HTTP/1.1 pipeline
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/CURLMOPT_MAX_PIPELINE_LENGTH.html
 */
mhandle::MHDL_RetCode
mhandle::set_max_pipeline_length(long max) noexcept
{
    return set_opt_long(CURLMOPT_MAX_PIPELINE_LENGTH, max);
}

/**
 * @brief set_max_pipeline_length - Set the maximum number of simultaneously open connections
 * When reaching the limit, the sessions will be pending until there are available connections
 * @param max The maximum amount of simultaneously open connections
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/CURLMOPT_MAX_TOTAL_CONNECTIONS.html
 */
mhandle::MHDL_RetCode
mhandle::set_max_total_connections(long max) noexcept
{
    return set_opt_long(CURLMOPT_MAX_TOTAL_CONNECTIONS, max);
}

/**
 * @brief set_maxconnects set the size of the connections cache
 * Setting this limit can prevent the cache from growing too much
 * @param max The maximum amount of cached connections
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/CURLMOPT_MAXCONNECTS.html
 */
mhandle::MHDL_RetCode
mhandle::set_maxconnects(long max) noexcept
{
    return set_opt_long(CURLMOPT_MAXCONNECTS, max);
}

/**
 * @brief set_pipelining - Enable/disable HTTP pipelining and multiplexing
 * @param mask Enable HTTP pipelining and/or HTTP/2 multiplexing for this multi handle.
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/CURLMOPT_PIPELINING.html
 * @see CURLPIPE_NOTHING | CURLPIPE_HTTP1 | CURLPIPE_MULTIPLEX
 */
mhandle::MHDL_RetCode
mhandle::set_pipelining(long mask) noexcept
{
    return set_opt_long(CURLMOPT_PIPELINING, mask);
}

/**
 * @brief set_pushfunction - Set the callback that approves or denies server pushes
 * @param cb The callback that is called when a new HTTP/2 stream is being pushed by the server
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/CURLMOPT_PUSHFUNCTION.html
 */
mhandle::MHDL_RetCode
mhandle::set_pushfunction(TCbPush cb) noexcept
{
    // TODO
}

/**
 * @brief set_pushdata - Set the pointer to pass to the push callback (\see set_pushfunction)
 * @param userdata Will be passed as the last argument to the pushfunction callback.
 * @return A return code described by the \a MHDL_RetCode enumerate
 *
 * @see https://curl.se/libcurl/c/CURLMOPT_PUSHDATA.html
 */
mhandle::MHDL_RetCode
mhandle::set_pushdata(void* userdata) noexcept
{
    return set_opt_ptr(CURLMOPT_PUSHDATA, userdata);
}

} // namespace asyncurl
