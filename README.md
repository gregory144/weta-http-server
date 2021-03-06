        ____     ____     ____   _____    __  ___
       / __ \   / __ \   /  _/  / ___/   /  |/  /
      / /_/ /  / /_/ /   / /    \__ \   / /|_/ /
     / ____/  / _, _/  _/ /    ___/ /  / /  / /
    /_/      /_/ |_|  /___/   /____/  /_/  /_/



<pre>
DATA TRACE [2014-11-04 10:52:34.436] 5052 4920 2a20 4854 5450 2f32 2e30 0d0a   <b>PRI</b> * HTTP/2.0..
DATA TRACE [2014-11-04 10:52:34.437] 0d0a 534d 0d0a 0d0a                       ..<b>SM</b>....
</pre>


An interoperable HTTP2 server.


[![Build Status](https://travis-ci.org/gregory144/prism-web-server.png)](https://travis-ci.org/gregory144/prism-web-server)

## Build from git

    mkdir build && cd build
    CC=clang cmake -DCMAKE_BUILD_TYPE=Release ..
    make

## Run

    openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout key.pem -out cert.pem
    ./bin/prism -L INFO -p ./lib/libfiles_plugin.so

## Run Tests

    make test

With valgrind:

    CK_TIMEOUT_MULTIPLIER=10 ctest -D ExperimentalMemCheck --output-on-failure -VV

## Run clang-analyzer

    mkdir build && cd build
    scan-build cmake -DCMAKE_BUILD_TYPE=Debug ..
    scan-build make

## Dependencies

For Ubuntu:

    apt-get install build-essential cmake libssl-dev flex bison doxygen check libjansson-dev libjansson4

* libuv (you'll have to build from source or find a package - there isn't one in the default repos - https://github.com/libuv/libuv)
* pthreads
* openssl libssl/libcrypto (1.0.2 for ALPN)
* jansson for config file parsing (json library)
* flex & bison
* check unit test framework

* joyent http parser [https://github.com/joyent/http-parser] (already in repo, see src/http/h1_1/http_parser.c)

## TODO

#### Required features

* serious security checks
  * fuzzer using test harness to randomly generate illegal requests
* configuration
  * load plugins for a specific listen address only
* graceful shutdown
  * second int signal should force shutdown
* better error handling for libuv calls
* doxygen documentation
* complete spec compliance
  * error on violation of incoming max concurrent streams
  * goaway on bad hpack indexes

#### Up next

* output data frame padding config option
* watch config file and update without restart
* access/error log
* switch from heap based priority queue to linked list?

* finite state machine for handling stream status
* plugin system that allows access to connections, streams, frames, requests, responses (callbacks)
  * plugin versioning system - each call has a version number that is checked on plugin load

#### Performance:

* fast huffman coding + decoding - more than one bit at a time
* better hpack encoding algorithm - use indexing
* stream priority
* files - cache file, if the file changes (libuv watch it), update the cache

* test - http_request_header_get after sending response headers - "accept-language" - does it get the right value?
