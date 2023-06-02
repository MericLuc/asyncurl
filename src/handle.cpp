/**
 * Copyright (C) 2023 Osmozis SA - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <asyncurl/handle.hpp>
#include <curl/curl.h>
#include <stdexcept>

namespace asyncurl
{
/*!
 * \brief default constructor
 */
handle::handle()
  : curl_handle__{ curl_easy_init() }
{
    if (nullptr == curl_handle__) throw std::runtime_error("Unable to created a CURL handle");

    set_option_ptr(CURLOPT_PRIVATE, this);
    set_option_bool(CURLOPT_NOSIGNAL, true); // multi-threaded applications
}

/*!
 * \brief destructor
 */
handle::~handle() noexcept
{
    if (nullptr != multi_handler__) multi_handler__->remove_handle(*this);
    if (nullptr != curl_handle__) curl_easy_cleanup(curl_handle__);
}

//---------------------------------------------------------------------------------------------------------------------
// UN/PAUSE TRANSFER
// \see https://curl.se/libcurl/c/curl_easy_pause.html
//---------------------------------------------------------------------------------------------------------------------

/*!
 * \brief pause pause the transfer in one or both directions
 * \param bitmask CURL pause mask (CURLPAUSE_RECV, CURLPAUSE_SEND, CURLPAUSE_ALL, CURLPAUSE_CONT)
 * \return true if the command was successfull, false otherwise
 */
bool
handle::pause(int bitmask) noexcept
{
    const auto prev{ flags__ };
    flags__ |= (bitmask & CURLPAUSE_ALL);
    return (flags__ == prev) || (CURLE_OK == curl_easy_pause(curl_handle__, flags__ & CURLPAUSE_ALL));
}

/*!
 * \brief is_paused test if the transfer is paused in one or both directions
 * \param bitmask CURL pause mask (CURLPAUSE_RECV, CURLPAUSE_SEND, CURLPAUSE_ALL, CURLPAUSE_CONT)
 * \return true if the transfer is paused in the required direction(s), false otherwise
 */
bool
handle::is_paused(int bitmask) noexcept
{
    return (0 != (flags__ & bitmask));
}

/*!
 * \brief unpause unpause the transfer in one or both directions
 * \param bitmask CURL pause mask (CURLPAUSE_RECV, CURLPAUSE_SEND, CURLPAUSE_ALL, CURLPAUSE_CONT)
 * \return true if the command was successfull, false otherwise
 */
bool
handle::unpause(int bitmask) noexcept
{
    const auto old{ flags__ };
    flags__ &= ~(bitmask & CURLPAUSE_ALL);
    return (flags__ == old) || (CURLE_OK == curl_easy_pause(curl_handle__, flags__ & CURLPAUSE_ALL));
}

//---------------------------------------------------------------------------------------------------------------------
// CALLBACKS
// \see https://everything.curl.dev/libcurl/callbacks
//---------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------------------
// OPTIONS
// Change specific socket options before the underlying socket is connected
// \see https://everything.curl.dev/libcurl/callbacks/sockopt
//---------------------------------------------------------------------------------------------------------------------

/**
 * \brief Get an information element (\see CURLINFO type) of type long
 * \param id The information's id to get
 * \return The requested information
 */
long
handle::get_info_long(int id) const
{}

/**
 * \brief Get an information element (\see CURLINFO type) of type long
 * \param id The information's id to get
 * \return The requested information
 */
uint64_t
handle::get_info_socket(int id) const
{}

/**
 * \brief Get an information element (\see CURLINFO type) of type long
 * \param id The information's id to get
 * \return The requested information
 */
double
handle::get_info_double(int id) const
{}

/**
 * \brief Get an information element of type string
 * \param id The information's id to get
 * \return The requested info (do not free the returned value)
 */
std::string
handle::get_info_string(int id) const
{}

/**
 * \brief Set an information element (\see CURLINFO type) of type long
 * \param id The information's id to set
 * \param val The value to set
 */
void
handle::set_option_long(int id, long val)
{}

/**
 * \brief Set an information element (\see CURLINFO type) of type offset
 * \param id The information's id to set
 * \param val The value to set
 */
void
handle::set_option_offset(int id, long val)
{}

/**
 * \brief Set an information element (\see CURLINFO type) of type ptr
 * \param id The information's id to set
 * \param val The value to set
 */
void
handle::set_option_ptr(int id, const void* val)
{}

/**
 * \brief Set an information element (\see CURLINFO type) of type string
 * \param id The information's id to set
 * \param val The value to set
 */
void
handle::set_option_string(int id, const char* val)
{}

/**
 * \brief Set an information element (\see CURLINFO type) of type bool
 * \param id The information's id to set
 * \param val The value to set
 */
void
handle::set_option_bool(int id, bool val)
{}

} // namespace asyncurl
