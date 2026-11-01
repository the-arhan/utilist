#include <core/application_controller.hpp>
#include <QApplication>
#include <iostream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

int main(int argc, char* argv[]) {
    // 1. Single-instance enforcement via Win32 Named Mutex
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Local\\UtilistSingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Another instance is already running
        std::cerr << "Another instance of UTILIST is already running." << std::endl;
        if (hMutex != nullptr && hMutex != INVALID_HANDLE_VALUE) {
            CloseHandle(hMutex);
        }
        return 0;
    }

    // 2. High DPI and Style configuration
    QApplication app(argc, argv);
    app.setApplicationName("UTILIST");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("UtilistPlatform");

    // 3. Application Controller
    utilist::app::core::ApplicationController controller;
    if (!controller.initialize()) {
        std::cerr << "Failed to initialize UTILIST Application Controller." << std::endl;
        CloseHandle(hMutex);
        return 1;
    }

    // Check command line for --minimized
    bool startMinimized = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--minimized") {
            startMinimized = true;
            break;
        }
    }

    if (!startMinimized) {
        controller.run();
    }

    int exitCode = app.exec();

    controller.shutdown();
    CloseHandle(hMutex);
    return exitCode;
}
