# asyncurl
**:star2: C++ library to perform multi-threaded asynchronous network transfers :star2:**

`asyncurl` is a C++ wrapper around `libcurl` providing interfaces to easily perform network transfers.

It links with the `miniloop` eventloop (sorry for libcurl eventloop agnosticity) to provide an event-driven interface.

# How to use it

`asyncurl` provide 2 structures : 
- [`handle`](include/asyncurl/handle.hpp) representing a single network transfer 
- [`mhandle`](include/asyncurl/mhandle.hpp) representing a session that can handle sevderal (thousands) simultaneous transfers

There are basically two ways to use this library - that are details in the [`example`](examples) folder :
1. synchronously (blocking)
2. asynchronously (event-driven)


## Blocking transfers

1. Create a single transfer `asyncurl::handle`
2. Setup its callbacks to perform you customs operations during the stages of the transfer :
  - read operations
  - write operations
  - transfer progress checking
  - end of transfer
  - debug
  - ...
3. Setup the behaviour of the transfer by modifying its options `handle::set_opt()`
4. Perform your transfer - just call `handle::perform_blocking()`

You are encouraged to reuse your `handle` as much as possible as it enables the higher performances.


## Asynchronous transfers

1. Setup your transfers by creating as mush of them as you want (see previous section).
2. Create a session to hold your transfers `asyncurl::mhandle`
3. If needed, modify the behaviour of the session by modifying its options `mhandle::set_opt()`
4. Add your transfers by calling `mhandle::add_handle()`

The session will work with the loop to perform your transfers.

Note that you can add/remove transfer(s) at any time. 

**Important notes**

Here are the basic rules to respect to make sure your transfers go smoothly :
  - Your loop instance must outlive your session !
  - You are not allowed to use the loop in another thread !

When a transfer is done, you might want to rerun it (as it, or by modifying before).

To do so, you should re-add your transfer to the session by successively calling `mhandle::remove_handle()` and `mhandle::add_handle()`.

**What about multi-threading?**

The exact same rules apply for multi-threading.

You need to setup one session by thread, that will use a loop dedicated to the thread. 


# Building and installing asyncurl

Simple, just type :

```
cmake -S path/to/src -DCMAKE_INSTALL_PREFIX=path/to/install -DCMAKE_BUILD_TYPE=Release
make install
```

You can also enable facultative targets by setting cmake variables 

| variable name     | type    | description
| ----------------- | ------- | ------------------ |
|  `BUILD_EXAMPLES` | boolean | Build the examples |
|  `BUILD_DOC`      | boolean | Build the doxygen documentation |
