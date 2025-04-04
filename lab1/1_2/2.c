#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

void usage(const char *progName) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <file1> [file2 ...] xorN\n", progName);
    fprintf(stderr, "  %s <file1> [file2 ...] mask <hex>\n", progName);
    fprintf(stderr, "  %s <file1> [file2 ...] copyN\n", progName);
    fprintf(stderr, "  %s <file1> [file2 ...] find <SomeString>\n", progName);
}

void doXor(char **fileNames, int fileCount, int N) {
    if (N == 2) {
        for (int i = 0; i < fileCount; i++) {
            FILE *fp = fopen(fileNames[i], "rb");
            if (!fp) {
                perror(fileNames[i]);
                continue;
            }
            int result = 0;
            int byteRead;
            while ((byteRead = fgetc(fp)) != EOF) {
                unsigned char byte = (unsigned char)byteRead;
                unsigned char highNibble = (byte >> 4) & 0x0F;
                unsigned char lowNibble  = byte & 0x0F;
                result ^= highNibble;
                result ^= lowNibble;
            }
            fclose(fp);
            printf("File: %s, XOR result (4 bits): %X\n", fileNames[i], result & 0x0F);
        }
    } else {
        int blockSize = (1 << N) / 8;
        for (int i = 0; i < fileCount; i++) {
            FILE *fp = fopen(fileNames[i], "rb");
            if (!fp) {
                perror(fileNames[i]);
                continue;
            }
            fseek(fp, 0, SEEK_END);
            long fsize = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            unsigned char *buffer = malloc(fsize);
            if (!buffer) {
                fclose(fp);
                fprintf(stderr, "Memory allocation failed\n");
                continue;
            }
            fread(buffer, 1, fsize, fp);
            fclose(fp);

            int numBlocks = (fsize + blockSize - 1) / blockSize;
            unsigned char *result = malloc(blockSize);
            if (!result) {
                free(buffer);
                fprintf(stderr, "Memory allocation failed\n");
                continue;
            }
            memset(result, 0, blockSize);

            for (int b = 0; b < numBlocks; b++) {
                for (int j = 0; j < blockSize; j++) {
                    unsigned char byte = 0x16;
                    int pos = b * blockSize + j;
                    if (pos < fsize)
                        byte = buffer[pos];
                    result[j] ^= byte;
                }
            }

            printf("File: %s, XOR result: ", fileNames[i]);
            for (int j = 0; j < blockSize; j++) {
                printf("%02X", result[j]);
            }
            printf("\n");
            free(buffer);
            free(result);
        }
    }
}

void doMask(char **fileNames, int fileCount, unsigned int mask) {
    for (int i = 0; i < fileCount; i++) {
        FILE *fp = fopen(fileNames[i], "rb");
        if (!fp) {
            perror(fileNames[i]);
            continue;
        }
        unsigned int count = 0;
        unsigned int value;
        while (fread(&value, sizeof(unsigned int), 1, fp) == 1) {
            if ((value & mask) == mask)
                count++;
        }
        fclose(fp);
        printf("File: %s, Count: %u\n", fileNames[i], count);
    }
}

void doCopy(char **fileNames, int fileCount, int N) {
    if (N <= 0) {
        fprintf(stderr, "Invalid copy number.\n");
        return;
    }
    for (int i = 0; i < fileCount; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Error creating process: fork\n");
            continue;
        }
        if (pid == 0) {
            FILE *src = fopen(fileNames[i], "rb");
            if (!src) {
                fprintf(stderr, "Failed to open file: %s\n", fileNames[i]);
                return;
            }
            fseek(src, 0, SEEK_END);
            long fsize = ftell(src);
            fseek(src, 0, SEEK_SET);
            unsigned char *buffer = malloc(fsize);
            if (!buffer) {
                fclose(src);
                fprintf(stderr, "Memory allocation failed\n");
                return;
            }
            fread(buffer, 1, fsize, src);
            fclose(src);

            for (int copy = 1; copy <= N; copy++) {
                char newName[512];
                snprintf(newName, sizeof(newName), "%s_copy%d", fileNames[i], copy);
                FILE *dest = fopen(newName, "wb");
                if (!dest) {
                    fprintf(stderr, "Failed to create file: %s\n", newName);
                    continue;
                }
                fwrite(buffer, 1, fsize, dest);
                fclose(dest);
            }

            free(buffer);
            return;
        }
    }
    for (int i = 0; i < fileCount; i++)
        wait(NULL);
}

void doFind(char **fileNames, int fileCount, const char *searchString) {
    for (int i = 0; i < fileCount; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Error creating process: fork\n");
            continue;
        }
        if (pid == 0) {
            FILE *fp = fopen(fileNames[i], "r");
            if (!fp) {
                perror(fileNames[i]);
                return;
            }
            int found = 0;
            char *line = NULL;
            size_t len = 0;
            ssize_t read;
            while ((read = getline(&line, &len, fp)) != -1) {
                if (strstr(line, searchString) != NULL) {
                    found = 1;
                    break;
                }
            }
            free(line);
            fclose(fp);

            if (found)
                printf("Found in: %s\n", fileNames[i]);
            else
                printf("String not found in: %s\n", fileNames[i]);
            return;
        }
    }
    for (int i = 0; i < fileCount; i++)
        wait(NULL);
}

int processCommand(int argc, char *argv[]) {
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }
    
    int flagIndex = -1;
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "xor", 3) == 0 ||
            strcmp(argv[i], "mask") == 0 ||
            strncmp(argv[i], "copy", 4) == 0 ||
            strcmp(argv[i], "find") == 0) {
            flagIndex = i;
            break;
        }
    }
    
    if (flagIndex == -1) {
        usage(argv[0]);
        return 1;
    }
    
    int fileCount = flagIndex - 1;
    if (fileCount < 1) {
        usage(argv[0]);
        return 1;
    }
    
    char **fileNames = &argv[1];
    char *flag = argv[flagIndex];
    char *extraParam = NULL;
    
    if ((strcmp(flag, "mask") == 0) || (strcmp(flag, "find") == 0)) {
        if (flagIndex + 1 >= argc) {
            usage(argv[0]);
            return 1;
        }
        extraParam = argv[flagIndex + 1];
    }
    
    if (strncmp(flag, "xor", 3) == 0) {
        int N;
        if (sscanf(&flag[3], "%d", &N) != 1 || N < 2 || N > 6) {
            fprintf(stderr, "Invalid N for xor operation. N must be in [2,6].\n");
            return 1;
        }
        doXor(fileNames, fileCount, N);
    }
    else if (strcmp(flag, "mask") == 0) {
        unsigned int mask;
        char *endptr;
        mask = strtol(extraParam, &endptr, 16);
        if (*endptr != '\0') {
            fprintf(stderr, "Invalid hex mask: %s\n", extraParam);
            return 1;
        }
        
        doMask(fileNames, fileCount, mask);
    }
    else if (strncmp(flag, "copy", 4) == 0) {
        int N;
        if (sscanf(&flag[4], "%d", &N) != 1 || N <= 0) {
            fprintf(stderr, "Invalid copy number: %s\n", &flag[4]);
            return 1;
        }
        doCopy(fileNames, fileCount, N);
    }
    else if (strcmp(flag, "find") == 0) {
        doFind(fileNames, fileCount, extraParam);
    }
    else {
        fprintf(stderr, "Unknown flag: %s\n", flag);
        usage(argv[0]);
        return 1;
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    return processCommand(argc, argv);
}
