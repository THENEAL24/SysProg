#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

#define MAX_FILES 1024

void printPermissions(mode_t mode) {
    char perms[11] = "----------";
    
    if (S_ISDIR(mode)) perms[0] = 'd';
    else if (S_ISLNK(mode)) perms[0] = 'l';
    else if (S_ISCHR(mode)) perms[0] = 'c';
    else if (S_ISBLK(mode)) perms[0] = 'b';
    else if (S_ISFIFO(mode)) perms[0] = 'p';
    else if (S_ISSOCK(mode)) perms[0] = 's';
    
    perms[1] = (mode & S_IRUSR) ? 'r' : '-';
    perms[2] = (mode & S_IWUSR) ? 'w' : '-';
    perms[3] = (mode & S_IXUSR) ? 'x' : '-';
    perms[4] = (mode & S_IRGRP) ? 'r' : '-';
    perms[5] = (mode & S_IWGRP) ? 'w' : '-';
    perms[6] = (mode & S_IXGRP) ? 'x' : '-';
    perms[7] = (mode & S_IROTH) ? 'r' : '-';
    perms[8] = (mode & S_IWOTH) ? 'w' : '-';
    perms[9] = (mode & S_IXOTH) ? 'x' : '-';
    
    printf("%s ", perms);
}

void printMetadata(const struct stat *fileStat, const char *fileName) {
    struct passwd *pw = getpwuid(fileStat->st_uid);
    struct group *gr = getgrgid(fileStat->st_gid);
    char timeStr[20];

    printPermissions(fileStat->st_mode);

    printf("%2hu ", fileStat->st_nlink);
    printf("%-8s %-8s ", pw ? pw->pw_name : "?", gr ? gr->gr_name : "?");
    printf("%8lld ", fileStat->st_size);

    struct tm *tmInfo = localtime(&fileStat->st_mtime);
    strftime(timeStr, sizeof(timeStr), "%b %d %H:%M", tmInfo);
    printf("%s ", timeStr);

    printf("%s\n", fileName);
}

int compareFilenames(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

void listDirectory(const char *path) {
    DIR *dir;
    struct dirent *entry;
    struct stat fileStat;
    char fullPath[4096];
    long totalBlocks = 0;
    char *fileNames[MAX_FILES];
    int count = 0;

    if ((dir = opendir(path)) == NULL) {
        fprintf(stderr, "Error opening directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (count >= MAX_FILES) break;

        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);

        if (lstat(fullPath, &fileStat) == 0) {
            totalBlocks += fileStat.st_blocks;

            char *name = strdup(entry->d_name);
            if (name == NULL) {
                fprintf(stderr, "Memory allocation failed");
                closedir(dir);
                return;
            }

            fileNames[count++] = name;
        } else {
            fprintf(stderr, "Failed to get file status");
        }
    }

    printf("total %ld\n", totalBlocks);

    qsort(fileNames, count, sizeof(char *), compareFilenames);

    for (int i = 0; i < count; i++) {
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, fileNames[i]);
        if (lstat(fullPath, &fileStat) == 0) {
            printMetadata(&fileStat, fileNames[i]);
        } else {
            fprintf(stderr, "Failed to get file status");
        }

        free(fileNames[i]);
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <directory>...\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        printf("\nListing contents of directory: %s\n", argv[i]);
        listDirectory(argv[i]);
    }

    return 0;
}
