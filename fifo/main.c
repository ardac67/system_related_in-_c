#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1000                    // buffer limit for student array
#define FLAGS O_CREAT | O_APPEND | O_WRONLY // File open flags
#define READ_FLAGS O_CREAT | O_APPEND       // File open flags
#define READ_PERMS S_IRUSR | S_IWUSR        // File permissions

void logMessage(char *message);                                                // log message function takes message as parameter and puts in the log file
int compareByName(const void *a, const void *b);                               // compare by name function
int compareByGrade(const void *a, const void *b);                              // compare by grade function
void openFile(char *filename);                                                 // open file function takes file name as parameter and opens the file
void searchStudent(char *name_search, char *search_surname, char *file_name);  // search student function takes name, surname and file name as parameter and searches the student
void addStudentGrade(char *name, char *surname, char *grade, char *file_name); // add student grade function takes name, surname, grade and file name as parameter and adds the student grade
void sortAll(int *file, char *order, char *type, char *file_name);             // sort all function takes file, order, type and file name as parameter and sorts the students -g or -n / -a or -d
void showAll(char *file_name);                                                 // show all function takes file name as parameter and shows all students
void listGrades(char *file_name);                                              // list grades function takes file name as parameter and lists first 5 students
void listSome(char *file_name, int page_num, int num_entries);                 // list some function takes file name, page number and number of entries as parameter and lists the students
 
 
static void sighandler(int signo) // signal handler for ctrl c // just for checking childs are dead or not
{
    if (signo == SIGINT)
    {
        printf("\nReceived SIGINT\n");
        printf("Exiting ....\n");
        char *message_first = "Process Terminated By User";
        logMessage(message_first);
        exit(EXIT_SUCCESS);
    }
}
void errExit(const char *message) // error exit function
{
    perror(message);
    exit(EXIT_FAILURE);
}

int sort_order = 1; // Global variable to store the sort order

typedef struct // Student struct
{
    char name[50];
    char surname[50];
    char grade[3];
} Student;


int main()
{
    int inputFd = -1;
    char input[256];
    char file_name[40];
    if (signal(SIGINT, sighandler) == SIG_ERR) // signal handler for ctrl c and it's error checking
    {
        errExit("signal");
    }

    while (1)
    {
        char *words[256];
        int wordCount = 0;
        printf("> ");
        fflush(stdout);
        ssize_t numRead;
        numRead = read(STDIN_FILENO, input, sizeof(input) - 1); // parsing file name and commands
        if (numRead > 0)
        {
            input[numRead] = '\0'; // Manually null-terminate the string
            input[strcspn(input, "\n")] = '\0';

            char *word = strtok(input, " ");
            while (word != NULL && wordCount < sizeof(words) / sizeof(words[0]))
            {
                words[wordCount] = strdup(word);
                if (words[wordCount] == NULL)
                {
                    fprintf(stderr, "Memory allocation failed\n");
                    return EXIT_FAILURE;
                }
                wordCount++;
                word = strtok(NULL, " ");
            }

            if (wordCount > 0 && strcmp(words[0], "gtuStudentGrades") == 0 && wordCount >= 2)
            {
                strcpy(file_name, words[1]);
                openFile(words[1]);
            }
            else if (strcmp(words[0], "gtuStudentGrades") == 0 && wordCount == 1)
            {
                printf("-----Usage-----\n");
                printf("$ gtuStudentGrades <file_name>\n");
                printf("$ addStudentGrade <name> <surname> <grade> <filename>\n");
                printf("$ searchStudent <name> <surname> <filename>\n");
                printf("$ sortAll <-n/-g> <-a/-d> <filename>\n");
                printf("$ showAll <filename>\n");
                printf("$ listGrades <filename>\n");
                printf("$ listSome <num_entries> <page_number> <filename>\n");
                printf("$ Ctrl + C to Exit\n");
            }
            else if (wordCount > 0 && strcmp(words[0], "addStudentGrade") == 0)
            {
                if (wordCount < 5)
                {
                    printf("Invalid command.\n");
                    continue;
                }
                addStudentGrade(words[1], words[2], words[3], words[4]);
            }
            else if (wordCount > 0 && strcmp(words[0], "searchStudent") == 0)
            {
                if (wordCount < 4)
                {
                    printf("Invalid command.\n");
                    continue;
                }
                searchStudent(words[1], words[2], words[3]);
            }
            else if (wordCount > 0 && strcmp(words[0], "sortAll") == 0)
            {
                if (wordCount < 4)
                {
                    printf("Invalid command.\n");
                    continue;
                }
                sortAll(&inputFd, words[2], words[1], words[3]);
            }
            else if (wordCount > 0 && strcmp(words[0], "showAll") == 0)
            {
                if (wordCount < 2)
                {
                    printf("Invalid command.\n");
                    continue;
                }
                showAll(words[1]);
            }
            else if (wordCount > 0 && strcmp(words[0], "listGrades") == 0)
            {
                if (wordCount < 2)
                {
                    printf("Invalid command.\n");
                    continue;
                }
                listGrades(words[1]);
            }
            else if (wordCount > 0 && strcmp(words[0], "listSome") == 0)
            {
                if (wordCount < 4)
                {
                    printf("Invalid command.\n");
                    continue;
                }
                listSome(words[3], atoi(words[2]), atoi(words[1]));
            }
            else
            {
                printf("Invalid command.\n");
            }

            for (int i = 0; i < wordCount; i++)
            {
                free(words[i]);
            }
        }
        else if (numRead == 0)
        {
            printf("EOF encountered.\n");
        }
        else
        {
            perror("Error reading input");
        }
    }
    close(inputFd);
    return 0;
}

// log message function takes message as parameter and puts in the log file
void logMessage(char *message)
{
    int openFlags = FLAGS;
    mode_t filePerms = READ_PERMS;
    int file_desc = open("log.txt", openFlags, filePerms);
    if (file_desc == -1)
        errExit("open");

    if (write(file_desc, message, strlen(message)) != strlen(message)) // writes to file
    {
        errExit("write");
    }
    write(file_desc, "\n", 1);
    if (close(file_desc) == -1)
        errExit("close");
}

int compareByName(const void *a, const void *b)
{
    const Student *studentA = (const Student *)a;
    const Student *studentB = (const Student *)b;
    int lastNameComp = strcmp(studentA->name, studentB->name);
    if (lastNameComp == 0)
    { // If names are equal, compare surnames
        return strcmp(studentA->surname, studentB->surname);
    }
    return lastNameComp * sort_order;
}

int compareByGrade(const void *a, const void *b)
{
    const Student *studentA = (const Student *)a;
    const Student *studentB = (const Student *)b;
    int gradeComp = strcmp(studentA->grade, studentB->grade);
    return gradeComp * sort_order;
}

void openFile(char *filename)
{
    pid_t pid = fork(); // makes fork
    if (pid == -1)
        errExit("fork");
    if (pid == 0) // checks child or not
    {
        int openFlags = FLAGS;
        mode_t filePerms = READ_PERMS;
        int file_desc = open(filename, openFlags, filePerms);
        if (file_desc == -1)
            errExit("open");
        else
        {
            char *message_first = "File opened successfully ->";
            int total_lenth = strlen(message_first) + strlen(filename) + 1;
            char *message = malloc(total_lenth);
            strcpy(message, message_first);
            strcat(message, filename);
            if (message == NULL)
            {
                errExit("Memory allocation failed");
            }
            logMessage(message);
            printf("File opened successfully.\n");
        }

        if (close(file_desc) == -1)
            errExit("close");

        exit(EXIT_SUCCESS); // terminates child
    }
    else if (pid > 0) // if parent waits for child
    {
        wait(NULL);
    }
}

void searchStudent(char *name_search, char *search_surname, char *file_name)
{
    if (strlen(file_name) == 0)
    {
        printf("Please open a file first.\n");
        return;
    }
    pid_t pid = fork(); // makes fork
    if (pid == -1)
    {
        errExit("fork");
    }
    if (pid == 0) // checks child or not
    {

        int count = 0;
        int openFlags = READ_FLAGS;
        mode_t filePerms = READ_PERMS;
        int file_desc = open(file_name, openFlags, filePerms);
        if (file_desc == -1)
            errExit("open");
        char buffer[BUFFER_SIZE];
        ssize_t numRead;
        Student students[BUFFER_SIZE]; //  array of students

        // Reading from the file descriptor
        while ((numRead = read(file_desc, buffer, BUFFER_SIZE - 1)) > 0) // reads from file and pardes it into student array
        {
            buffer[numRead] = '\0'; // Ensure the buffer is null-terminated

            char *ctx;
            char *line = strtok_r(buffer, "\n", &ctx);
            while (line != NULL)
            {
                if (sscanf(line, "%49s %49s %2s", students[count].name, students[count].surname, students[count].grade) == 3)
                {
                    count++;
                }
                line = strtok_r(NULL, "\n", &ctx);
            }
        }
        if (close(file_desc) == -1) // closes used descriptor
            errExit("close");
        int count_lines = 0;
        int found = 0;
        while (count_lines < count) // prints the student that searched
        {
            if (strcmp(students[count_lines].name, name_search) == 0 && strcmp(students[count_lines].surname, search_surname) == 0)
            {
                printf("%s %s's grade: %s\n", name_search, search_surname, students[count_lines].grade);
                char *message_first = "Student searched ->";
                int total_lenth = strlen(message_first) + strlen(students[count_lines].name) + strlen(students[count_lines].surname) + strlen(" ") + 1;
                char *message = malloc(total_lenth);
                strcpy(message, message_first);
                strcat(message, students[count_lines].name);
                strcat(message, " ");
                strcat(message, students[count_lines].surname);
                if (message == NULL)
                {
                    errExit("Memory allocation failed");
                }
                logMessage(message);
                found = 1;
                break;
            }
            count_lines++;
        }

        if (!found) // if not found notifies user
        {
            printf("Student %s %s not found.\n", name_search, search_surname);
            char *message_first = "Student not found. ->";
            int total_lenth = strlen(message_first) + strlen(name_search) + strlen(search_surname) + strlen(" ") + 1;
            char *message = malloc(total_lenth);
            strcpy(message, message_first);
            strcat(message, name_search);
            strcat(message, " ");
            strcat(message, search_surname);
            if (message == NULL)
            {
                errExit("Memory allocation failed");
            }
            logMessage(message);
        }

        exit(EXIT_SUCCESS); // terminates child
    }
    else if (pid > 0)
    {
        wait(NULL); // Parent waits for the child to complete
    }
}

void addStudentGrade(char *name, char *surname, char *grade, char *file_name)
{
    if (strlen(file_name) == 0)
    {
        printf("Please open a file first.\n");
        return;
    }
    pid_t pid = fork(); // makes fork
    if (pid == -1)
        errExit("fork");
    if (pid == 0)
    {
        int openFlags = FLAGS;
        mode_t filePerms = READ_PERMS;
        int file_desc = open(file_name, openFlags, filePerms);
        if (file_desc == -1)
            errExit("open");

        char *line = malloc(256);
        if (line == NULL)
        {
            errExit("Memory allocation failed");
        }
        sprintf(line, "%s %s %s\n", name, surname, grade);
        if (write(file_desc, line, strlen(line)) != strlen(line)) // writes the student into file
        {
            errExit("write");
        }
        char *message_first = "Student Grade Added ->";
        int total_lenth = strlen(message_first) + strlen(name) + strlen(surname) + strlen(grade) + 3; // calculates total length of message
        char *message = malloc(total_lenth);
        strcpy(message, message_first);
        strcat(message, name);
        strcat(message, " ");
        strcat(message, surname);
        strcat(message, " ");
        strcat(message, grade);
        if (message == NULL)
        {
            errExit("Memory allocation failed");
        }
        logMessage(message); // logs what happened
        free(line);

        if (close(file_desc) == -1) // closes the descriptor
            errExit("close");

        exit(EXIT_SUCCESS); // terminates the child
    }
    else if (pid > 0)
    {
        wait(NULL);
    }
}

void sortAll(int *file, char *order, char *type, char *file_name)
{
    if (strlen(file_name) == 0)
    {
        printf("Please open a file first.\n");
        return;
    }
    pid_t pid = fork(); // creates child
    if (pid == -1)
        errExit("fork");
    else if (pid > 0) // Parent waits for the child to complete
    {
        wait(NULL);
    }
    else if (pid == 0) // Child process
    {
        int count = 0;
        int openFlags = READ_FLAGS;
        mode_t filePerms = READ_PERMS;
        int file_desc = open(file_name, openFlags, filePerms);
        if (file_desc == -1)
            errExit("open");
        char buffer[BUFFER_SIZE];
        ssize_t numRead;
        Student students[BUFFER_SIZE];

        // Reading from the file descriptor
        while ((numRead = read(file_desc, buffer, BUFFER_SIZE - 1)) > 0) // reads student into student array
        {
            buffer[numRead] = '\0';
            char *ctx;
            char *line = strtok_r(buffer, "\n", &ctx);
            while (line != NULL)
            {
                if (sscanf(line, "%49s %49s %2s", students[count].name, students[count].surname, students[count].grade) == 3)
                {
                    count++;
                }
                line = strtok_r(NULL, "\n", &ctx);
            }
        }
        if (close(file_desc) == -1)
            errExit("close");

        if (strcmp(type, "-n") == 0) // sorts according to name
        {
            if (strcmp(order, "-a") == 0) // ascending
                sort_order = 1;
            else if (strcmp(order, "-d") == 0) // descending
                sort_order = -1;
            qsort(students, count, sizeof(Student), compareByName); // sort function to make sort happen
            char *message_first = "Student are sorted by name succesfully!";
            logMessage(message_first); // logs what happened
        }
        else if (strcmp(type, "-g") == 0) // sorts according to grade
        {
            if (strcmp(order, "-a") == 0) // ascending
                sort_order = 1;
            else if (strcmp(order, "-d") == 0) // descending
                sort_order = -1;
            qsort(students, count, sizeof(Student), compareByGrade); // sort function to make sort happen
            char *message_first = "Student are sorted by grade succesfully!";
            logMessage(message_first); // logs what happened
        }

        for (int i = 0; i < count; i++)
        {
            printf("%s %s %s\n", students[i].name, students[i].surname, students[i].grade);
            int total_lenth = strlen(students[i].name) + strlen(students[i].surname) + strlen(students[i].grade) + 3;
            //logging what happened
            char *message = malloc(total_lenth);
            strcat(message, students[i].name);
            strcat(message, " ");
            strcat(message, students[i].surname);
            strcat(message, " ");
            strcat(message, students[i].grade);
            if (message == NULL)
            {
                errExit("Memory allocation failed");
            }
            logMessage(message);
            // prints sorted version but not changes main file
        }

        exit(EXIT_SUCCESS);
    }
}

void showAll(char *file_name)
{
    if (file_name == NULL)
    {
        printf("Please open a file first.\n");
        return;
    }
    pid_t pid = fork(); // creates child
    if (pid == -1)
    {
        errExit("fork");
    }
    if (pid == 0) // checks child or not
    {

        int count = 0;
        int openFlags = READ_FLAGS;
        mode_t filePerms = READ_PERMS;
        int file_desc = open(file_name, openFlags, filePerms);
        if (file_desc == -1)
            errExit("open");
        char buffer[BUFFER_SIZE];
        ssize_t numRead;
        Student students[BUFFER_SIZE];

        // Reading from the file descriptor
        while ((numRead = read(file_desc, buffer, BUFFER_SIZE - 1)) > 0) // reads file into student array
        {
            buffer[numRead] = '\0';

            char *ctx;
            char *line = strtok_r(buffer, "\n", &ctx);
            while (line != NULL)
            {
                if (sscanf(line, "%49s %49s %2s", students[count].name, students[count].surname, students[count].grade) == 3)
                {
                    count++;
                }
                line = strtok_r(NULL, "\n", &ctx);
            }
        }
        if (close(file_desc) == -1)
            errExit("close");
        int count_lines = 0;
        int found = 0;
        while (count_lines < count)
        {
            // prints the all students in the file which also in the array
            printf("%s %s's grade: %s\n", students[count_lines].name, students[count_lines].surname, students[count_lines].grade);
            count_lines++;
        }
        char *message_first = "All students are printed";
        logMessage(message_first);

        exit(EXIT_SUCCESS); // terminates child
    }
    else if (pid > 0)
    {
        wait(NULL); // Parent waits for the child to complete
    }
}

void listGrades(char *file_name)
{
    if (strlen(file_name) == 0)
    {
        printf("Please open a file first.\n");
        return;
    }
    pid_t pid = fork(); // creates child for the process
    if (pid == -1)
    {
        errExit("fork");
    }
    if (pid == 0) // checks child or not
    {

        int count = 0;
        int openFlags = READ_FLAGS;
        mode_t filePerms = READ_PERMS;
        int file_desc = open(file_name, openFlags, filePerms);
        if (file_desc == -1)
            errExit("open");
        char buffer[BUFFER_SIZE];
        ssize_t numRead;
        Student students[BUFFER_SIZE];
        while ((numRead = read(file_desc, buffer, BUFFER_SIZE - 1)) > 0) // Reading from the file descriptor
        {
            buffer[numRead] = '\0';

            char *ctx;
            char *line = strtok_r(buffer, "\n", &ctx);
            while (line != NULL)
            {
                // storing child into array
                if (sscanf(line, "%49s %49s %2s", students[count].name, students[count].surname, students[count].grade) == 3)
                {
                    count++;
                }
                line = strtok_r(NULL, "\n", &ctx);
                if (count == 5)
                {
                    break;
                }
            }
        }
        if (close(file_desc) == -1)
            errExit("close");
        int count_lines = 0;
        int found = 0;
        while (count_lines < count)
        {
            // printing just 5 of them
            printf("%s %s's grade: %s\n", students[count_lines].name, students[count_lines].surname, students[count_lines].grade);
            count_lines++;
        }
        char *message_first = "First 5 student are printed";
        logMessage(message_first); // logs what happened
        exit(EXIT_SUCCESS);        // terminates child
    }
    else if (pid > 0)
    {
        wait(NULL); // Parent waits for the child to complete
    }
}

void listSome(char *file_name, int page_num, int num_entries)
{
    if (strlen(file_name) == 0)
    {
        printf("Please open a file first.\n");
        return;
    }
    pid_t pid = fork(); // creates child
    if (pid == -1)
    {
        errExit("fork");
    }
    if (pid == 0) // checks child or not
    {

        int count = 0;
        int openFlags = READ_FLAGS;
        mode_t filePerms = READ_PERMS;
        int file_desc = open(file_name, openFlags, filePerms);
        if (file_desc == -1)
            errExit("open");
        char buffer[BUFFER_SIZE];
        ssize_t numRead;
        Student students[BUFFER_SIZE];
        int startLine = (5 * page_num) - 5;        // calculates the which line has to be started
        int endLine = startLine + num_entries - 1; // find the last line should printed
        // Reading from the file descriptor
        while ((numRead = read(file_desc, buffer, BUFFER_SIZE - 1)) > 0)
        {
            buffer[numRead] = '\0';
            char *ctx;
            char *line = strtok_r(buffer, "\n", &ctx);
            while (line != NULL)
            {
                // reads file into student array
                if (sscanf(line, "%49s %49s %2s", students[count].name, students[count].surname, students[count].grade) == 3)
                {
                    count++;
                }
                line = strtok_r(NULL, "\n", &ctx);
            }
        }
        // gets the lines between start and end
        while (startLine <= endLine)
        {
            printf("%s %s's grade: %s\n", students[startLine].name, students[startLine].surname, students[startLine].grade);
            startLine++;
        }
        char *message_first = "Students are listed by page number";
        logMessage(message_first);  // logs what happende
        if (close(file_desc) == -1) // closes descriptor and checks if it's closed or not
            errExit("close");

        exit(EXIT_SUCCESS); // terminates child
    }
    else if (pid > 0)
    {
        wait(NULL); // Parent waits for the child to complete
    }
}
