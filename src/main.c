#include <stdbool.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define TERM_CURSOR_LEFT "\e[%dD"

struct Size {
    int w, h;
};
static struct Size _termSize;

struct FileList;

struct File {
    char* filename;
    bool isDir;
    struct File* next;
};

struct FileList {
    struct File* head;
    struct File* tail;
    size_t count;
};

static
void FileList_Add(struct FileList* l, struct File* f)
{
    f->next = NULL;
    if(l->head) {
        l->tail->next = f;
        l->tail = f;
    } else {
        l->head = f;
        l->tail = f;
    }
    l->count++;
}

static
bool loadFileList(struct FileList* fl, const char* path, time_t* lastProgress)
{
    DIR* dir = opendir(path);
    if(!dir) {
        perror("opendir");
        fprintf(stderr, "Path: \"%s\"\n", path);
        return false;
    }

    struct dirent* entry;
    while((entry = readdir(dir)) != NULL) {
        if(strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            /* Contents of a directory must be listed before it */
            size_t filenameSize = strlen(path) + entry->d_namlen + 2;   /* zero terminator + one optional slash */
            char* filename = malloc(filenameSize);
            strcpy(filename, path);
            if(strlen(filename) && filename[strlen(filename) - 1] != '/') {
                strcat(filename, "/");
            }
            strcat(filename, entry->d_name);

            if(entry->d_type == DT_DIR) {
                bool ret = loadFileList(fl, filename, lastProgress);
                if(!ret) {
                    closedir(dir);
                    return false;
                }
            } else {
                struct File* f = malloc(sizeof(struct File));
                f->filename = filename;
                f->isDir = entry->d_type == DT_DIR;
                FileList_Add(fl, f);
            }
        }

        time_t now = time(NULL);
        if(now - *lastProgress > 1) {
            printf("\r%zu...", fl->count);
            *lastProgress = now;
        }
    }
    closedir(dir);

    struct File* f = malloc(sizeof(struct File));
    f->filename = strdup(path);
    f->isDir = true;
    FileList_Add(fl, f);
    return true;
}

static
void progress(int value, int max, time_t elapsed)
{
    /* Max size of status text */
    char status_text[256];
    snprintf(status_text, sizeof(status_text), "%d", max);
    int number_width = strlen(status_text);

    /*
     * value -> elapsed
     * max   -> (max * elapsed) / value
     */
    time_t eta;
    if(value)
        eta = (max * elapsed) / value;
    else
        eta = 0;

    char selapsed[32];
    strftime(selapsed, sizeof(selapsed), "%H:%M:%S", gmtime(&elapsed));

    char seta[32];
    strftime(seta, sizeof(seta), "%H:%M:%S", gmtime(&eta));

    snprintf(status_text, sizeof(status_text),
             " %*d/%d Elapsed %s ETA %s", 
             number_width, value, max, selapsed, seta);
    size_t status_text_size = strlen(status_text);

    /*
     * max -> term_width
     * value -> (value * term_width) / max
     */
    printf(TERM_CURSOR_LEFT, 1000);
    int width = (value * (_termSize.w - 2 - status_text_size)) / max;
    printf("[");
    for(int i = 0; i < width; i++) {
        printf("█");
    }
    for(int i = 0; i < (_termSize.w - 2 - status_text_size) - width; i++) {
        printf(" ");
    }
    printf("]%s", status_text);
    fflush(stdout);
}

static
bool recursiveDelete(const struct FileList* lst, time_t startTime)
{
    printf("\n");

    time_t lastProgress = startTime;
    size_t currentCount = 0;
    struct File* f;
    for(f = lst->head; f; f = f->next)  {
        if(f->isDir) {
            int ret = rmdir(f->filename);
            if(ret) {
                perror("rmdir");
                return false;
            }
        } else {
            int ret = unlink(f->filename);
            if(ret) {
                perror("unlink");
                return false;
            }
        }

        currentCount++;

        time_t now = time(NULL);
        if(now - lastProgress > 1) {
            progress(currentCount, lst->count, now - startTime);
            lastProgress = now;
        }
    }
    return true;
}

static
struct Size termSize()
{
    struct winsize ws;
    int fd = open("/dev/tty", O_RDWR);
    if(fd == -1) {
        struct Size result = { 80, 25 };
        return result;
    }
    int ret = ioctl(fd, TIOCGWINSZ, &ws);
    if(ret == -1) {
        int err = errno;
        close(fd);
        errno = err;

        struct Size result = { 80, 25 };
        return result;
    }

    close(fd);

    struct Size result = { 80, 25 };
    result.w = ws.ws_col >= 0 ? ws.ws_col : 80;
    result.h = ws.ws_row >= 0 ? ws.ws_row : 25;

    return result;
}

static
void onSigwinch(int signal)
{
    _termSize = termSize();
}

static
void usage(FILE* f, const char* program)
{
    fprintf(f,
            "Usage: %s <filename>\n",
            program);
}

int main(int argc, char** argv)
{
    if(argc != 2) {
        usage(stdout, argv[0]);
        return 1;
    }

    /* Init terminal handling */
    _termSize = termSize();
    signal(SIGWINCH, onSigwinch);

    /* Load list of files so we can show progressbar */
    const char* path = argv[1];
    printf("Scanning \"%s\"\n", path);

    time_t lastProgress = 0;
    struct FileList lst = {0};
    bool ret = loadFileList(&lst, path, &lastProgress);
    if(!ret) {
        return 1;
    }
    printf("\nTotal files: %zu\n", lst.count);

    /* Recursive delete */
    time_t startTime = time(NULL);
    ret = recursiveDelete(&lst, startTime);
    if(!ret) {
        return 1;
    }
    progress(lst.count, lst.count, time(NULL) - startTime);

    return 0;
}

