#include <windows.h>
#include <stdio.h>
#include "RoxanneMovementLibrary.c" // Include the movement library

// FIFO queue for hand commands
#define QUEUE_SIZE 10 // Maximum number of commands in queue
HandCommand command_queue[QUEUE_SIZE];
int queue_front = 0;
int queue_rear = 0;
int queue_count = 0;

// Initialize the command queue
void init_queue() {
    queue_front = 0;
    queue_rear = 0;
    queue_count = 0;
    printf("Command queue initialized. Ready to add commands.\n");
}

// Add a hand command to the queue
void add_to_queue(HandCommand cmd) {
    if (queue_count < QUEUE_SIZE) {
        command_queue[queue_rear] = cmd;
        queue_rear = (queue_rear + 1) % QUEUE_SIZE;
        queue_count++;
        printf("Command added to queue.\n");
    } else {
        printf("Queue is full. Cannot add more commands.\n");
    }
}

// Execute all commands in the queue sequentially
void execute_queue() {
    printf("Executing queued commands...\n");
    while (queue_count > 0) {
        HandCommand cmd = command_queue[queue_front];
        queue_front = (queue_front + 1) % QUEUE_SIZE;
        queue_count--;
        execute_hand_command(cmd); // Execute the command (defined in RoxanneMovementLibrary.c)
    }
    printf("All queued commands executed.\n");
}

// Print available commands
void print_available_commands() {
    printf("Hand Control CLI Initialized\n");
    printf("Available commands:\n");
    printf("  init       - Initialize the command queue\n");
    printf("  open_hand  - Add 'open hand' command to queue\n");
    printf("  close_hand - Add 'close hand' command to queue\n");
    printf("  execute    - Execute all queued commands sequentially\n");
    printf("  exit       - Exit the program\n");
    printf("Enter commands:\n");
}
