#include <stdio.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#endif

unsigned short enable_win_ansi() {
// Nothing but the return statement is included in non-Windows builds
#ifdef _WIN32
    HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (console_handle == INVALID_HANDLE_VALUE) {
        return 0;
    }

    // Get console settings
    DWORD prev_mode = 0;
    GetConsoleMode(console_handle, (LPDWORD)&prev_mode);
    if (prev_mode == 0) {
        return 0;
    }

    // Enable VT100 emulation, which allows ANSI escape codes
    DWORD mode = prev_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
    SetConsoleMode(console_handle, mode); // Apply the change
#endif
    return 1;
}

int logging_print(char* type, char* function, char* format_str, ...) {
    // Print "__func__(): " with function name in color and the rest in white
    printf("\033[%sm%s\033[0m(): ", type, function);

    // We use helpers from stdarg.h to handle the variadic (...) arguments.
    va_list arg_list = {0};
    va_start(arg_list, format_str);
    int return_code = vprintf(format_str, arg_list);
    va_end(arg_list);

    return return_code;
}

