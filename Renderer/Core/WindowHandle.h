
#pragma once

namespace LX {
    struct WindowHandle {
        #if defined(_WIN32)
            void* hwnd;
            void* hinstance;
        #endif
        #if defined(__linux__)
            void* window;
            void* connection;
        #endif
    };
}