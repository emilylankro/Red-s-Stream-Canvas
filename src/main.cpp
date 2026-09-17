#include "App.h"

#include <windows.h>
#include <winrt/base.h>
#include <string>
#include <exception>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        App app(instance);
        return app.Run(showCommand);
    } catch (const winrt::hresult_error& error) {
        const std::wstring message = L"Red's Stream Canvas failed to start.\n\n" + std::wstring(error.message());
        MessageBoxW(nullptr, message.c_str(), L"Red's Stream Canvas", MB_OK | MB_ICONERROR);
        return static_cast<int>(error.code().value);
    } catch (const std::exception& error) {
        const std::wstring message = L"Red's Stream Canvas failed to start.\n\nUnexpected error.";
        MessageBoxW(nullptr, message.c_str(), L"Red's Stream Canvas", MB_OK | MB_ICONERROR);
        return 1;
    }
}
