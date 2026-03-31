#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "RoxanneCommander.c" // Include the commander for queue functions

// Function to open serial port (hardcoded to COM3, change as needed)
int open_serial() {
    hSerial = CreateFile("\\\\.\\COM3", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) {
        printf("Error opening COM3\n");
        return 0;
    }
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(hSerial, &dcb)) {
        printf("Error getting comm state\n");
        CloseHandle(hSerial);
        return 0;
    }
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    if (!SetCommState(hSerial, &dcb)) {
        printf("Error setting comm state\n");
        CloseHandle(hSerial);
        return 0;
    }
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    if (!SetCommTimeouts(hSerial, &timeouts)) {
        printf("Error setting timeouts\n");
        CloseHandle(hSerial);
        return 0;
    }
    printf("Serial port COM3 opened at 115200 baud.\n");
    return 1;
}

// Function to close serial port
void close_serial() {
    if (hSerial != INVALID_HANDLE_VALUE) {
        CloseHandle(hSerial);
        printf("Serial port closed.\n");
    }
}

int main() {
    if (!open_serial()) {
        return 1;
    }
    print_available_commands();

    char input[100];
    while (1) {
        printf("> ");
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0; // Remove newline

        if (strcmp(input, "init") == 0) {
            init_queue();
        } else if (strcmp(input, "open_hand") == 0) {
            add_to_queue(open_hand);
        } else if (strcmp(input, "close_hand") == 0) {
            add_to_queue(close_hand);
        } else if (strcmp(input, "execute") == 0) {
            execute_queue();
        } else if (strcmp(input, "exit") == 0) {
            break;
        } else if (strlen(input) > 0) {
            printf("Unknown command: %s\n", input);
            printf("Type 'init', 'open_hand', 'close_hand', 'execute', or 'exit'\n");
        }
    }
    close_serial();
    return 0;
}
