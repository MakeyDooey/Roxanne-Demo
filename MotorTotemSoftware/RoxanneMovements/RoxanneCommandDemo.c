#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <setupapi.h>
#include <devguid.h>
#include <initguid.h>
#include "RoxanneCommander.c" // Include the commander for queue functions

const GUID GUID_DEVINTERFACE_COMPORT = {0x86E0D1E0, 0x8089, 0x11D0, {0x9C, 0xE4, 0x08, 0x00, 0x3E, 0x30, 0x1F, 0x73}};

// Function to open serial port (auto-detect ESP32-S3-DevKit1 COM port)
int open_serial() {
    HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        printf("Error getting device info\n");
        return 0;
    }

    SP_DEVICE_INTERFACE_DATA devInterfaceData;
    devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    char comPort[256] = {0};
    DWORD dwIndex = 0;
    BOOL found = FALSE;

    printf("Scanning for COM ports...\n");

    while (SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &GUID_DEVINTERFACE_COMPORT, dwIndex, &devInterfaceData)) {
        DWORD dwSize = 0;
        SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInterfaceData, NULL, 0, &dwSize, NULL);

        PSP_DEVICE_INTERFACE_DETAIL_DATA pDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(dwSize);
        if (pDetailData) {
            pDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

            SP_DEVINFO_DATA devInfoData;
            devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

            if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInterfaceData, pDetailData, dwSize, &dwSize, &devInfoData)) {
                char hardwareId[256] = {0};
                char friendlyName[256] = {0};
                SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_HARDWAREID, NULL, (PBYTE)hardwareId, sizeof(hardwareId), NULL);
                SetupDiGetDeviceRegistryProperty(hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME, NULL, (PBYTE)friendlyName, sizeof(friendlyName), NULL);

                printf("Found COM port: %s - Hardware ID: %s\n", friendlyName, hardwareId);

                if (strstr(hardwareId, "VID_10C4&PID_EA60") || strstr(hardwareId, "VID_303A&PID_1001") || strstr(hardwareId, "VID_0403") || strstr(hardwareId, "VID_067B")) {
                    // Found ESP32 or USB-to-UART device
                    char* comStart = strstr(friendlyName, "(COM");
                    if (comStart) {
                        char* comEnd = strstr(comStart, ")");
                        if (comEnd) {
                            *comEnd = '\0';
                            strcpy(comPort, comStart + 1); // skip (
                            found = TRUE;
                            printf("Device detected on %s\n", comPort);
                        }
                    }
                }
            }
            free(pDetailData);
        }
        if (found) break;
        dwIndex++;
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

    if (!found || strlen(comPort) == 0) {
        printf("ESP32-S3-DevKit1 not found on any COM port (looking for VID_10C4&PID_EA60 or VID_303A&PID_1001)\n");
        return 0;
    }

    char fullPath[256];
    sprintf(fullPath, "\\\\.\\%s", comPort);

    hSerial = CreateFile(fullPath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == 5) { // Access denied
            printf("Port in use, attempting to kill conflicting processes...\n");
            system("taskkill /f /im \"Arduino IDE.exe\" /t >nul 2>&1");
            system("taskkill /f /im arduino-cli.exe /t >nul 2>&1");
            system("taskkill /f /im arduino-language-server.exe /t >nul 2>&1");
            system("taskkill /f /im clangd.exe /t >nul 2>&1");
            system("taskkill /f /im serial-discovery.exe /t >nul 2>&1");
            system("taskkill /f /im serial-monitor.exe /t >nul 2>&1");
            Sleep(2000); // Wait for processes to die
            hSerial = CreateFile(fullPath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            if (hSerial == INVALID_HANDLE_VALUE) {
                error = GetLastError();
                printf("Still failed to open %s: %lu\n", comPort, error);
                return 0;
            }
        } else {
            printf("Error opening %s: %lu\n", comPort, error);
            return 0;
        }
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
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
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
    EscapeCommFunction(hSerial, SETDTR);
    Sleep(1000); // Wait for ESP32 to stabilize
    printf("Serial port %s opened at 115200 baud.\n", comPort);
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
