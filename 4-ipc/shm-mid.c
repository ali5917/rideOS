// =========================================================
// Multi-Stage Student Attendance System using Shared Memory
// 
// 1. Attendance Producer: Writes 5 raw attendance records (1 for Present, 0 for Absent) to shared memory.
// 2. Attendance Analyzer: Reads the records, calculates total present/absent, and writes the summary back.
// 3. Attendance Reporter: Reads the summary, prints it to the console, and saves it to attendance_report.txt.
// =========================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define SHM_NAME "/attendance_shm"
#define SHM_SIZE (8 * sizeof(int))

// Memory Layout for the integer array:
// ptr[0] to ptr[4]: Attendance records (1 for Present, 0 for Absent)
// ptr[5]: total_present
// ptr[6]: total_absent
// ptr[7]: status (Used for synchronization: 0=Empty, 1=Produced, 2=Analyzed)

int main() {
    int fd;
    int *ptr;
    pid_t pid1, pid2;

    // Create shared memory object
    fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    // Set size
    ftruncate(fd, SHM_SIZE);

    // Map memory as an integer array
    ptr = (int *)mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    
    ptr[7] = 0; // Initial state

    // ----------------- FIRST FORK -----------------
    pid1 = fork();

    if (pid1 > 0) {
        // ---------------- ATTENDANCE PRODUCER ----------------
        printf("Attendance Producer: Sending attendance records...\n");
        int raw_attendance[5] = {1, 0, 1, 1, 0}; // 1 = Present, 0 = Absent
        
        for (int i = 0; i < 5; i++) {
            ptr[i] = raw_attendance[i];
        }
        
        ptr[5] = 0; // total_present
        ptr[6] = 0; // total_absent
        ptr[7] = 1; // Signal Analyzer that data is ready

        wait(NULL); // Wait for Analyzer to finish
        
        // Final Cleanup by Parent
        munmap(ptr, SHM_SIZE);
        close(fd);
        shm_unlink(SHM_NAME);
    } 
    else {
        // ---------------- SECOND FORK -----------------
        pid2 = fork();

        if (pid2 > 0) {
            // ---------------- ATTENDANCE ANALYZER ----------------
            // Spinlock: Wait until Producer is done writing
            while(ptr[7] != 1) { usleep(1000); }

            printf("Attendance Analyzer: Calculating attendance summary...\n");
            int present = 0, absent = 0;
            for (int i = 0; i < 5; i++) {
                if (ptr[i] == 1) present++;
                else absent++;
            }

            ptr[5] = present;
            ptr[6] = absent;
            ptr[7] = 2; // Signal Reporter that summary is ready

            wait(NULL); // Wait for Reporter to finish
            
            munmap(ptr, SHM_SIZE);
            close(fd);
        } 
        else {
            // ---------------- ATTENDANCE REPORTER ----------------
            // Spinlock: Wait until Analyzer is done calculating
            while(ptr[7] != 2) { usleep(1000); }

            printf("Attendance Reporter: Displaying and logging attendance summary...\n");
            
            // Display to Console
            printf("Total Students Present: %d\n", ptr[5]);
            printf("Total Students Absent: %d\n", ptr[6]);
            printf("System: Attendance processed and logged successfully.\n");

            // Save to File
            // Save to File using system calls
            int file_fd = open("attendance_report.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (file_fd != -1) {
                char buffer[256];
                sprintf(buffer, "Total Students Present: %d\nTotal Students Absent: %d\n", ptr[5], ptr[6]);
                write(file_fd, buffer, strlen(buffer));
                close(file_fd);
            } else {
                perror("open file");
            }
            
            munmap(ptr, SHM_SIZE);
            close(fd);
        }
    }

    return 0;
}