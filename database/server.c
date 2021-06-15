#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netinet/in.h>

#define DATA_BUFFER 300

int curr_fd = -1;
int curr_id = -1;
const int SIZE_BUFFER = sizeof(char) * DATA_BUFFER;

const char *currDir = "/home/frain8/Documents/Sisop/FP/database/databases";
const char *USERS_TABLE = "./config/users.csv";

// Socket setup
int create_tcp_server_socket();
int *makeDaemon(pid_t *pid, pid_t *sid);

// Routes & controller
void *routes(void *argv);
bool login(int fd, char *username, char *password);
void regist(int fd, char *username, char *password);

// Helper
int getInput(int fd, char *prompt, char *storage);
int getUserId(const char *path, char *username, char *password);
int getLastId(const char *path);
void parseQuery(char *, char [20][50]);

int main()
{
    // TODO:: uncomment on final
    // pid_t pid, sid;
    // int *status = makeDaemon(&pid, &sid);

    socklen_t addrlen;
    struct sockaddr_in new_addr;
    pthread_t tid;
    char buf[DATA_BUFFER];
    int server_fd = create_tcp_server_socket();
    int new_fd;

    while (1) {
        new_fd = accept(server_fd, (struct sockaddr *)&new_addr, &addrlen);
        if (new_fd >= 0) {
            printf("Accepted a new connection with fd: %d\n", new_fd);
            pthread_create(&tid, NULL, &routes, (void *) &new_fd);
        } else {
            fprintf(stderr, "Accept failed [%s]\n", strerror(errno));
        }
    } /* while(1) */
    return 0;
}

void *routes(void *argv)
{
    chdir(currDir); // TODO:: comment on final
    int fd = *(int *) argv;
    char query[DATA_BUFFER], buf[DATA_BUFFER];
    char parsed[20][50];

    while (read(fd, query, DATA_BUFFER) != 0) {
        puts(query);
        parseQuery(query, parsed);

        if (strcmp(parsed[0], "LOGIN") == 0) {
            if (!login(fd, parsed[1], parsed[2]))
                break;
        }
        else if (strcmp(parsed[0], "CREATE") == 0) {
            if (strcmp(parsed[1], "USER") == 0) {
                if (curr_id == 0) {
                    regist(fd, parsed[2], parsed[5]);
                } else {
                    write(fd, "Forbidden action\n\n", SIZE_BUFFER);
                }
            }
            else {
                write(fd, "Invalid query on CREATE command\n\n", SIZE_BUFFER);
            }
        }
        else {
            write(fd, "Invalid query\n\n", SIZE_BUFFER);
        }
    }
    if (fd == curr_fd) {
        curr_fd = curr_id = -1;
    }
    printf("Close connection with fd: %d\n", fd);
    close(fd);
}

/****   Controllers   *****/
void regist(int fd, char *username, char *password)
{
    FILE *fp = fopen(USERS_TABLE, "a");
    if (fp == NULL) {
        fp = fopen(USERS_TABLE, "a+");
        fprintf(fp, "id,username,password\n");
    }
    int id = getUserId(USERS_TABLE, username, password);

    if (id != -1) {
        write(fd, "Error::User is already registered\n\n", SIZE_BUFFER);
    } else {
        id = getLastId(USERS_TABLE) + 1;
        fprintf(fp, "%d,%s,%s\n", id, username, password);
        write(fd, "Register success\n\n", SIZE_BUFFER);
    }
    fclose(fp);
}

bool login(int fd, char *username, char *password)
{
    if (curr_fd != -1) {
        write(fd, "Server is busy, wait for other user to logout.\n", SIZE_BUFFER);
        return false;
    }

    int id = -1;
    if (strcmp(username, "root") == 0) {
        id = 0;
    } else { // Check data in DB
        FILE *fp = fopen(USERS_TABLE, "r");
        if (fp != NULL) id = getUserId(USERS_TABLE, username, password);
        fclose(fp);
    }

    if (id == -1) {
        write(fd, "Error::Invalid id or password\n", SIZE_BUFFER);
        return false;
    } else {
        write(fd, "Login success\n", SIZE_BUFFER);
        curr_fd = fd;
        curr_id = id;
    }
    return true;
}

/*****  HELPER  *****/
int getUserId(const char *path, char *username, char *password)
{
    int id = -1;
    FILE *fp = fopen(path, "r");

    if (fp != NULL) {
        char db[DATA_BUFFER], input[DATA_BUFFER];
        sprintf(input, "%s,%s", username, password);

        while (fscanf(fp, "%s", db) != EOF) {
            char *temp = strstr(db, ",") + 1; // Get username and password from db
            if (strcmp(temp, input) == 0) {
                id = atoi(strtok(db, ","));  // Get id from db
                break;
            }
        }
        fclose(fp);
    }
    return id;
}

int getLastId(const char *path)
{
    int id = 1;
    FILE *fp = fopen(path, "r");

    if (fp != NULL) {
        char db[DATA_BUFFER];
        while (fscanf(fp, "%s", db) != EOF) {
            id = atoi(strtok(db, ","));  // Get id from db
        }
    }
    return id;
}

void parseQuery(char query[], char storage[20][50])
{
    char *buf = query;
    char *temp = NULL;
    memset(storage, '\0', sizeof(char) * 20 * 50);

    int i = 0;
    while ((temp = strtok(buf, " ")) != NULL && i < 20) {
        if (buf != NULL) {
            buf = NULL;
        }
        strcpy(storage[i++], temp);
    }

    // Remove ";""
    char *ptr = strstr(storage[--i], ";");
    if (ptr != NULL) {
        *ptr = '\0';
    }
}


/****   SOCKET SETUP    *****/
int create_tcp_server_socket()
{
    struct sockaddr_in saddr;
    int fd, ret_val;
    int opt = 1;

    /* Step1: create a TCP socket */
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == -1) {
        fprintf(stderr, "socket failed [%s]\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    printf("Created a socket with fd: %d\n", fd);

    /* Initialize the socket address structure */
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(7000);
    saddr.sin_addr.s_addr = INADDR_ANY;

    /* Step2: bind the socket to port 7000 on the local host */
    ret_val = bind(fd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (ret_val != 0) {
        fprintf(stderr, "bind failed [%s]\n", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }

    /* Step3: listen for incoming connections */
    ret_val = listen(fd, 5);
    if (ret_val != 0) {
        fprintf(stderr, "listen failed [%s]\n", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }
    return fd;
}

int *makeDaemon(pid_t *pid, pid_t *sid)
{
    int status;
    *pid = fork();

    if (*pid != 0) {
        exit(EXIT_FAILURE);
    }
    if (*pid > 0) {
        exit(EXIT_SUCCESS);
    }
    umask(0);

    *sid = setsid();
    if (*sid < 0 || chdir(currDir) < 0) {
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    return &status;
}
