// =========================================================
// Multi-Stage Student Grading System using Named Pipes (FIFOs)
// 
// 1. Score Producer: Sends 6 raw scores through the first named pipe.
// 2. Grade Calculator: Reads scores, computes letter grades, and sends results through a second pipe.
// 3. Result Reporter: Reads grades, prints them to the console, and saves them to grades_report.txt.
// =========================================================

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

// Named pipes
#define PIPE1 "/tmp/pipe1"
#define PIPE2 "/tmp/pipe2"

// Function to convert score to grade (FAST-NUCES scheme)
void getGrade(int score, char *grade) {
    if (score >= 85)
        strcpy(grade, "A");
    else if (score >= 75)
        strcpy(grade, "A-");
    else if (score >= 60)
        strcpy(grade, "B");
    else if (score >= 50)
        strcpy(grade, "C");
    else if (score >= 40)
        strcpy(grade, "D");
    else
        strcpy(grade, "F");
}

int main() {
    int pipe1_fd, pipe2_fd;
    pid_t pid1, pid2;

    // Create named pipes
    mkfifo(PIPE1, 0666);
    mkfifo(PIPE2, 0666);

    // ----------------- FIRST FORK -----------------
    pid1 = fork();

    if (pid1 > 0) {
        // ---------------- SCORE PRODUCER (PARENT) ----------------
        int scores[] = {90, 78, 69, 53, 47, 35};
        printf("Score Producer: Sending student scores...\n");
        
        pipe1_fd = open(PIPE1, O_WRONLY);
        write(pipe1_fd, scores, sizeof(scores));
        close(pipe1_fd);
        
        wait(NULL); // Wait for Child
        wait(NULL); // Ensure all processes finish
        
        printf("System: All student grades processed and logged successfully.\n");
        
        // Cleanup pipes from filesystem
        unlink(PIPE1);
        unlink(PIPE2);
    }
    else {
        // ---------------- SECOND FORK -----------------
        pid2 = fork();
        
        if (pid2 > 0) {
            // ---------------- GRADE CALCULATOR (CHILD) ----------------
            int scores[6];
            int pipe2_fd_write;
            
            pipe1_fd = open(PIPE1, O_RDONLY);
            read(pipe1_fd, scores, sizeof(scores));
            close(pipe1_fd);
            
            printf("Grade Calculator: Calculating grades...\n");
            
            pipe2_fd_write = open(PIPE2, O_WRONLY);
            char buffer[256];
            
            for (int i = 0; i < 6; i++) {
                char grade[3];
                getGrade(scores[i], grade);
                sprintf(buffer, "Student %d Score: %d Grade: %s\n", i + 1, scores[i], grade);
                write(pipe2_fd_write, buffer, strlen(buffer) + 1);
            }
            
            close(pipe2_fd_write);
            wait(NULL); // Wait for Grandchild
        }
        else {
            // ---------------- RESULT REPORTER (GRANDCHILD) ----------------
            char buffer[256];
            FILE *file = fopen("grades_report.txt", "w");
            
            pipe2_fd = open(PIPE2, O_RDONLY);
            printf("Result Reporter: Displaying and logging student grades...\n");
            
            while (read(pipe2_fd, buffer, sizeof(buffer)) > 0) {
                printf("%s", buffer);
                fprintf(file, "%s", buffer);
            }
            
            close(pipe2_fd);
            fclose(file);
        }
    }

    return 0;
}