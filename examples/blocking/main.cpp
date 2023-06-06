#include <asyncurl/asyncurl.hpp>
#include <iostream>

#include <curl/curl.h>

using namespace asyncurl;

int
main()
{
    // Create our single transfer
    handle hdl;

    // Setup callbacks
    hdl.set_cb_read([](char* buff, size_t sz) -> size_t {
        std::cout << "read... " << (int)sz << " bytes\n";
        return sz;
    });
    hdl.set_cb_write([](char* buff, size_t sz) -> size_t {
        std::cout << "write... " << (int)sz << " bytes\n";
        return sz;
    });
    hdl.set_cb_done(
      [](int rc) { std::cout << "DONE : '" << handle::retCode2Str(static_cast<handle::HDL_RetCode>(rc)) << "'\n"; });

    // Setup options
    hdl.set_opt(CURLOPT_HTTPGET, 1L);
    hdl.set_opt(CURLOPT_URL, std::string("http://www.google.com"));
    hdl.set_opt(CURLOPT_VERBOSE, 0L);

    // Perform the transfer with the blocking interface
    hdl.perform_blocking();

    return EXIT_SUCCESS;
}
