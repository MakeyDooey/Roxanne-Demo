#include <windows.h>
#include <stdio.h>
#include <string.h>

// Serial port handle (global, opened in main)
HANDLE hSerial;

// Function to send a command string to the serial port
void send_command(const char* cmd) {
    DWORD bytes_written;
    WriteFile(hSerial, cmd, strlen(cmd), &bytes_written, NULL);
    printf("Sent: %s", cmd); // Debug output

    // Read response
    char buffer[256];
    DWORD bytes_read;
    COMMTIMEOUTS timeouts;
    GetCommTimeouts(hSerial, &timeouts);
    timeouts.ReadIntervalTimeout = 100; // Short timeout for response
    SetCommTimeouts(hSerial, &timeouts);
    if (ReadFile(hSerial, buffer, sizeof(buffer) - 1, &bytes_read, NULL) && bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("Received: %s", buffer);
    }
}

// Define structures for motor actions and hand commands
typedef struct {
    int motor_index; // 0, 1, 2 for the three motors/fingers
    const char* direction; // "CW" or "CCW"
    int speed; // Steps per second
    int steps; // Number of steps to move
} MotorAction;

typedef struct {
    MotorAction actions[3]; // Up to 3 motors can move simultaneously
    int num_actions; // Number of motors involved in this command
    int pause_ms; // Pause in milliseconds after executing this command
} HandCommand;

// Predefined hand position commands
// Example: Open hand - all fingers extend (CW direction)
HandCommand open_hand = {
    .actions = {
        {0, "CW", 100, 200}, // Motor 0: CW, 100 steps/sec, 200 steps
        {1, "CW", 100, 200}, // Motor 1: CW, 100 steps/sec, 200 steps
        {2, "CW", 100, 200}  // Motor 2: CW, 100 steps/sec, 200 steps
    },
    .num_actions = 3,
    .pause_ms = 500 // Pause 500ms in open position
};

// Example: Close hand - all fingers retract (CCW direction)
HandCommand close_hand = {
    .actions = {
        {0, "CCW", 100, 200}, // Motor 0: CCW, 100 steps/sec, 200 steps
        {1, "CCW", 100, 200}, // Motor 1: CCW, 100 steps/sec, 200 steps
        {2, "CCW", 100, 200}  // Motor 2: CCW, 100 steps/sec, 200 steps
    },
    .num_actions = 3,
    .pause_ms = 500 // Pause 500ms in closed position
};

// Add more predefined commands here as needed, e.g., point_finger, peace_sign, etc.
// Example:
// HandCommand point_finger = { ... };

// Function to execute a single hand command
// Sends START commands for all motors, waits for movement time, sends STOP, then pauses
void execute_hand_command(HandCommand cmd) {
    // Send START for each motor
    for (int i = 0; i < cmd.num_actions; i++) {
        MotorAction act = cmd.actions[i];
        char start_cmd[20];
        sprintf(start_cmd, "START:%d:%s\n", act.motor_index, act.direction);
        send_command(start_cmd);
    }
    // Calculate movement time: max time among actions
    int max_time_ms = 0;
    for (int i = 0; i < cmd.num_actions; i++) {
        MotorAction act = cmd.actions[i];
        int time_ms = (act.steps * 1000) / act.speed; // steps / (steps/sec) * 1000 = ms
        if (time_ms > max_time_ms) max_time_ms = time_ms;
    }
    Sleep(max_time_ms); // Wait for movement to complete
    // Send STOP
    send_command("STOP\n");
    // Pause after command
    Sleep(cmd.pause_ms);
}
