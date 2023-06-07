/**
 * @file main.cpp
 * @brief This is an example of how to use asyncurl to perform asynchronous (i.e. non-blocking) network transfers.
 * To do so, we will use the event-driven interface of asyncurl.
 * Basically, the workflow is supposed to look like this :
 * <ul>
 * <li>1 - Setup a session to hold the transfer (\see asyncurl::mhandle) </li>
 * <li>2 - Setup one or more single transfer (\see asyncurl::handle) </li>
 * <li>3 - Add your transfer to the session and let your loop do the magic </li>
 * </ul>
 *
 * In this example, we setup a transfer to download the README of asyncurl 5 times :)
 */

#include <Loop.h>
#include <asyncurl/asyncurl.hpp>

#include <curl/curl.h> // Convenient to get access to handle options (CURLOPT)
#include <fstream>     // Write to file
#include <iostream>
#include <signal.h>
#include <string.h>

using namespace asyncurl;
using namespace loop;

#define OUTPUT_FILENAME "nonblocking_output"
#define URL "https://lhm-pc.osmozisdev.com/common/docs/asyncurl"

int
main()
{
    // 0 - We setup everything we need
    Loop myLoop;

    auto sigintEvt = Loop::UNIX_SIGNAL(SIGINT, myLoop);
    sigintEvt.onEvent([&myLoop](int) {
        std::cout << strsignal(SIGINT) << std::endl;
        myLoop.exit();
    });

    auto sigtermEvt = Loop::UNIX_SIGNAL(SIGTERM, myLoop);
    sigtermEvt.onEvent([&myLoop](int) {
        std::cout << strsignal(SIGTERM) << std::endl;
        myLoop.exit();
    });

    std::ofstream outputFile{ OUTPUT_FILENAME, std::ios_base::out | std::ios_base::trunc };
    if (!outputFile.is_open())
    {
        std::cerr << "Unable to create output file '" << OUTPUT_FILENAME << std::endl;
        return EXIT_FAILURE;
    }

    // 1 - Setup our session
    mhandle sess = mhandle(myLoop);

    // 2 - Setup our transfer
    handle hdl;
    hdl.set_cb_write([&outputFile](char* buff, size_t sz) -> size_t {
        outputFile.write(buff, sz);
        return sz;
    });
    hdl.set_cb_done([&hdl, &sess, &outputFile](int rc) {
        static int i{ 0 };
        std::cout << "[DONE][" << i << "] - " << handle::retCode2Str(static_cast<handle::HDL_RetCode>(rc)) << std::endl;

        if (i++ < 5)
            sess.add_handle(hdl);
        else
            outputFile.close();
    });
    hdl.set_opt(CURLOPT_HTTPGET, 1L);
    hdl.set_opt(CURLOPT_URL, std::string(URL));
    hdl.set_opt(CURLOPT_VERBOSE, 0L);

    // 3 - Perform our transfer by adding it to the session
    sess.add_handle(hdl);

    myLoop.run();

    return EXIT_SUCCESS;
}
