/**
 * @file main.cpp
 * @brief This is an example of how to use asyncurl to perform synchronous (i.e. blocking) network transfers
 * Basically, the workflow is supposed to look like this :
 * <ul>
 * <li>1 - Create a single transfer (\see asyncurl::handle) </li>
 * <li>2 - Setup its callbacks to perform you customs operations during the stages of the transfer (reads/write
 * operations, end of transfer...) </li>
 * <li>3 - Setup the behaviour of the transfer by modifying its options (\see asyncurl::set_opt) </li>
 * <li>4 - Perform your transfer by calling asyncurl::handle::perform_blocking() </li>
 * </ul>
 *
 * In this example, we setup a transfer to download the README of this project :)
 */

#include <asyncurl/asyncurl.hpp>
#include <curl/curl.h> // Convenient to get access to handle options (CURLOPT)
#include <fstream>     // Write to file
#include <iostream>

using namespace asyncurl;

#define OUTPUT_FILENAME "output.txt"
#define URL "https://raw.githubusercontent.com/MericLuc/asyncurl/v1/README.md"

int
main()
{
    // 0 - We will start by setup the file that will contain the content of the response to our request
    std::ofstream outputFile{ OUTPUT_FILENAME, std::ios_base::out | std::ios_base::trunc };
    if (!outputFile.is_open())
    {
        std::cerr << "Unable to create output file '" << OUTPUT_FILENAME << "'\n";
        return EXIT_FAILURE;
    }

    // 1 - Create our single transfer
    handle hdl;

    // 2 - Setup callbacks
    hdl.set_cb_read([](char* /*buff*/, size_t sz) -> size_t {
        std::cout << "[read] - " << (int)sz << " bytes\n";
        return sz;
    });
    hdl.set_cb_write([&outputFile](char* buff, size_t sz) -> size_t {
        std::cout << "[write] - " << (int)sz << " bytes\n";
        outputFile.write(buff, sz);
        return sz;
    });
    hdl.set_cb_progress([](int64_t dl_total, int64_t dl_now, int64_t up_total, int64_t up_now) -> int {
        std::cout << "[progress] \n";
        std::cout << "\t-download " << dl_now << "/" << dl_total << " bytes\n";
        std::cout << "\t-upload " << up_now << "/" << up_total << " bytes\n";
        return 0;
    });
    hdl.set_cb_done([&outputFile](int rc) {
        std::cout << "[DONE] - '" << handle::retCode2Str(static_cast<handle::HDL_RetCode>(rc)) << "'\n";
        outputFile.close();
    });

    // 3 - Setup options
    hdl.set_opt(CURLOPT_HTTPGET, 1L);
    hdl.set_opt(CURLOPT_URL, std::string(URL));
    hdl.set_opt(CURLOPT_VERBOSE, 0L);

    // 4 - Perform the transfer with the blocking interface
    hdl.perform_blocking();

    return EXIT_SUCCESS;
}
