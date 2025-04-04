#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>

#define MAX_ATTEMPTS 5
#define MAX_COMMAND_LEN 256
#define DATABASE_FILE "users.dat"

typedef struct {
    char login[7];
    int pin;
    int maxCommands;
    int counter;
} User;

typedef struct {
    User *users;
    int count;
    int capacity;
} UserDatabase;

int initDatabase(UserDatabase *db, int initialCapacity) {
    db->users = malloc(initialCapacity * sizeof(User));
    if (!db->users) {
        return 1;
    }

    db->count = 0;
    db->capacity = initialCapacity;

    return 0;
}

void saveDatabaseToFile(UserDatabase *db) {
    FILE *file = fopen(DATABASE_FILE, "wb");
    if (!file) {
        fprintf(stderr, "Failed to open file for saving\n");
        return;
    }

    fwrite(&db->count, sizeof(int), 1, file);
    fwrite(db->users, sizeof(User), db->count, file);

    fclose(file);
}

void loadDatabaseFromFile(UserDatabase *db) {
    FILE *file = fopen(DATABASE_FILE, "rb");
    if (!file) {
        fprintf(stderr, "Database file not found. Starting with an empty database.\n");
        return;
    }

    fread(&db->count, sizeof(int), 1, file);

    db->capacity = db->count > 10 ? db->count : 10;
    db->users = realloc(db->users, db->capacity * sizeof(User));
    if (!db->users) {
        fprintf(stderr, "Error loading users: insufficient memory!\n");
        fclose(file);
        return;
    }

    fread(db->users, sizeof(User), db->count, file);
    fclose(file);
}

int isLoginUnique(UserDatabase *db, const char *login) {
    for (int i = 0; i < db->count; i++) {
        if (strcmp(db->users[i].login, login) == 0) {
            return 0;
        }
    }
    return 1;
}

int initUser(UserDatabase *db) {
    char login[256];
    int pin;

    int count = 0;

    if (db->count >= db->capacity) {
        db->capacity *= 2;
        User *tmp = realloc(db->users, db->capacity * sizeof(User));
        if (!tmp) {
            return 1;
        }
        db->users = tmp;
    }

    while (1) {
        printf("Enter login (max 6 characters, alphanumeric): ");
        
        if (count == 0) {
            while (getchar() != '\n');
            count++;
        }

        if (fgets(login, sizeof(login), stdin) == NULL) {
            fprintf(stderr, "Error: Failed to read login.\n");
            continue;
        }

        login[strcspn(login, "\n")] = '\0';

        if (strlen(login) < 1 || strlen(login) > 6) {
            fprintf(stderr, "Error: login must be at least 1 character and no more than 6!\n");
            continue;
        }

        int valid = 1;
        for (size_t i = 0; i < strlen(login); i++) {
            if (!isalnum(login[i])) {
                valid = 0;
                break;
            }
        }

        if (!valid) {
            fprintf(stderr, "Error: login must contain only alphanumeric characters!\n");
            continue;
        }

        if (!isLoginUnique(db, login)) {
            fprintf(stderr, "Error: This login is already taken!\n");
            continue;
        }

        break;
    }

    while (1) {
        printf("Enter PIN (must be between 0 and 100000): ");
        if (scanf("%d", &pin) != 1) {
            fprintf(stderr, "Invalid input! PIN must be a number.\n");
            while (getchar() != '\n');
            continue;
        }

        if (pin < 0 || pin > 100000) {
            fprintf(stderr, "Error: pin must be between 0 and 100000!\n");
            while (getchar() != '\n');
            continue;
        } else {
            break;
        }
    }

    strlcpy(db->users[db->count].login, login, sizeof(db->users[db->count].login));

    db->users[db->count].pin = pin;
    db->users[db->count].maxCommands = -1;
    db->users[db->count].counter = 0;
    db->count++;

    saveDatabaseToFile(db);

    return 0;
}

User* logUser(UserDatabase *db) {
    char login[7];
    int pin;

    printf("Enter login: ");
    scanf("%6s", login);
    while (getchar() != '\n');

    printf("Enter PIN: ");
    scanf("%d", &pin);
    while (getchar() != '\n');

    for (int i = 0; i < db->count; i++) {
        if (strcmp(db->users[i].login, login) == 0 && db->users[i].pin == pin) {
            printf("\nAuthorization successful! Welcome, %s.\n", login);
            return &db->users[i];
        }
    }

    return NULL;
}

void printTime(void) {
    time_t t;
    struct tm *tm_info;
    char buffer[9];

    time(&t);
    tm_info = localtime(&t);
    strftime(buffer, sizeof(buffer), "%H:%M:%S", tm_info);
    printf("Current time: %s\n", buffer);
}

void printDate(void) {
    time_t t;
    struct tm *tm_info;
    char buffer[11];

    time(&t);
    tm_info = localtime(&t);
    strftime(buffer, sizeof(buffer), "%d:%m:%Y", tm_info);
    printf("Current date: %s\n", buffer);
}

void howmuchElapsedTime(char *inputTime, char *flag) {
    struct tm tm_time = {0};
    time_t currentTime, elapsedTime;
    double seconds;

    if (strptime(inputTime, "%d-%m-%Y", &tm_time) == NULL) {
        fprintf(stderr, "Invalid date format. Please use dd-mm-yyyy.\n");
        return;
    }

    elapsedTime = mktime(&tm_time);
    if (elapsedTime == -1) {
        fprintf(stderr, "Failed to convert date to time.\n");
        return;
    }

    currentTime = time(NULL);
    if (currentTime == -1) {
        fprintf(stderr, "Failed to get current time.\n");
        return;
    }

    seconds = difftime(currentTime, elapsedTime);

    if (strcmp(flag, "-s") == 0) {
        printf("Time passed: %.0f seconds\n", fabs(seconds));
    } else if (strcmp(flag, "-m") == 0) {
        printf("Time passed: %.0f minutes\n", fabs(seconds / 60));
    } else if (strcmp(flag, "-h") == 0) {
        printf("Time passed: %.0f hours\n", fabs(seconds / 3600));
    } else if (strcmp(flag, "-y") == 0) {
        printf("Time passed: %.0f years\n", fabs(seconds / (3600 * 24 * 365.25)));
    } else {
        fprintf(stderr, "Unknown flag.\n");
    }
}

int setSanctions(const char *username, const char *number, UserDatabase *db) {
    if (!username || !number || !db) {
        fprintf(stderr, "Error: invalid arguments.\n");
        return 1;
    }

    char *endptr;
    int num = strtol(number, &endptr, 10);
    if (*endptr != '\0' || num < 1) {
        fprintf(stderr, "Error: number must be a positive integer.\n");
        return 1;
    }

    User *targetUser = NULL;
    for (int i = 0; i < db->count; i++) {
        if (strcmp(db->users[i].login, username) == 0) {
            targetUser = &db->users[i];
            break;
        }
    }

    if (!targetUser) {
        fprintf(stderr, "Error: user with this login not found.\n");
        return 1;
    }

    int confirmation;
    printf("Confirm sanctions for user %s by entering 12345: ", username);
    scanf("%d", &confirmation);

    while (getchar() != '\n');

    if (confirmation == 12345) {
        targetUser->maxCommands = num;
        printf("Sanctions for user %s successfully set. Maximum commands: %d.\n", username, num);
    } else {
        fprintf(stderr, "Error confirming sanctions.\n");
        return 1;
    }

    saveDatabaseToFile(db);
    
    return 0;
}

void freeDatabase(UserDatabase *db) {
    free(db->users);
}

int processCommand(char *command, UserDatabase *db, User **currentUser) {
    char cmd[50], arg1[50], arg2[50];
    int numArgs = sscanf(command, "%s %s %s", cmd, arg1, arg2);
    int numSanctions = (*currentUser)->maxCommands;
    int counter = (*currentUser)->counter;
    (*currentUser)->counter++;

    if (counter < numSanctions || numSanctions == -1) {
        if (strcmp(cmd, "time") == 0) {
            printTime();
        } else if (strcmp(cmd, "date") == 0) {
            printDate();
        } else if (strcmp(cmd, "howmuch") == 0 && numArgs == 3) {
            howmuchElapsedTime(arg1, arg2);
        } else if (strcmp(cmd, "logout") == 0) {
            (*currentUser)->counter = 0;
            saveDatabaseToFile(db);
            printf("Logging out...\n");
            *currentUser = NULL;
        } else if (strcmp(cmd, "sanctions") == 0 && numArgs == 3) {
            setSanctions(arg1, arg2, db);
        } else if (strcmp(cmd, "exit") == 0) {
            (*currentUser)->counter = 0;
            saveDatabaseToFile(db);
            printf("Exiting program...\n");
            freeDatabase(db);
            return 1;
        } else {
            fprintf(stderr, "Unknown command.\n");
        }
    } else if (strcmp(cmd, "exit") == 0) {
        (*currentUser)->counter = 0;
        saveDatabaseToFile(db);
        printf("Exiting program...\n");
        freeDatabase(db);
        return 1;
    } else if (strcmp(cmd, "logout") == 0) {
        (*currentUser)->counter = 0;
        saveDatabaseToFile(db);
        printf("Logging out...\n");
        *currentUser = NULL;
    } else {
        fprintf(stderr, "Error: request limit exceeded\n");
    }

    return 0;
}

void menu(void) {
    UserDatabase db;
    if (initDatabase(&db, 10) != 0) {
        fprintf(stderr, "Program initialization failed. Exiting...\n");
        return;
    }

    loadDatabaseFromFile(&db);

    User *currentUser = NULL;
    char command[MAX_COMMAND_LEN];
    int choice;

    while (1) {
        if (currentUser == NULL) {
            printf("\n==== MENU ====\n");
            printf("1. Log in\n");
            printf("2. Register\n");
            printf("3. Exit\n");
            printf("Enter your choice: ");

            if (scanf("%d", &choice) != 1) {
                fprintf(stderr, "Invalid option. Please enter 1, 2, or 3.\n");
                while (getchar() != '\n');
                continue;
            }

            switch (choice) {
                case 1:
                    currentUser = logUser(&db);
                    if (currentUser == NULL) {
                        fprintf(stderr, "Error: incorrect login or PIN\n");
                    }
                    break;
                case 2:
                    if (initUser(&db) == 0) {
                        printf("User registered successfully!\n");
                    }
                    break;
                case 3:
                    saveDatabaseToFile(&db);
                    freeDatabase(&db);
                    return;
                default:
                    fprintf(stderr, "Invalid option. Please select a valid option from the menu.\n");
                    break;
            } 
        } else {
            printf("\n==== LIST OF COMMANDS ====\n");
            printf("time, date, howmuch <time> flag, logout, sanctions username <number>, exit\n");
            printf("Enter command: ");
            fgets(command, MAX_COMMAND_LEN, stdin);

            if (processCommand(command, &db, &currentUser)) {
                return;
            }
        }
    }
}

int main(void) {
    menu();
    return 0;
}
