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
#include <dirent.h>

#define DATA_BUFFER 200

int curr_fd = -1;
int curr_id = -1;
char curr_db[DATA_BUFFER] = {0};

const int SIZE_BUFFER = sizeof(char) * DATA_BUFFER;
const char *currDir = "/home/frain8/Documents/Sisop/FP/database/databases";

// Socket setup
int create_tcp_server_socket();
int *makeDaemon(pid_t *pid, pid_t *sid);

// Routes & controller
void *routes(void *argv);
bool login(int fd, char *username, char *password);
void regist(int fd, char *username, char *password);
void useDB(int fd, char *db_name);
void grantDB(int fd, char *db_name, char *username);
void createDB(int fd, char *db_name);
void createTable(int fd, char parsed[20][DATA_BUFFER]);

// Services
int getInput(int fd, char *prompt, char *storage);
int getUserId(char *username, char *password);
int getLastId(char *db_name, char *table);
void explode(char *string, char storage[20][DATA_BUFFER], const char *delimiter);
void changeCurrDB(int fd, const char *db_name);
FILE *getTable(char *db_name, char *table, char *cmd, char *collumns);
bool dbExist(int fd, char *db_name, bool printError);

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
    char parsed[20][DATA_BUFFER];

    while (read(fd, query, DATA_BUFFER) != 0) {
        puts(query);
        explode(query, parsed, " ");

        if (strcmp(parsed[0], "LOGIN") == 0) {
            if (!login(fd, parsed[1], parsed[2]))
                break;
        }
        else if (strcmp(parsed[0], "CREATE") == 0) {
            if (strcmp(parsed[1], "USER") == 0) {
                regist(fd, parsed[2], parsed[5]);
            }
            else if (strcmp(parsed[1], "DATABASE") == 0) {
                createDB(fd, parsed[2]);
            }
            else if (strcmp(parsed[1], "TABLE") == 0) {
                createTable(fd, parsed);
            }
            else write(fd, "Invalid query on CREATE command\n\n", SIZE_BUFFER);
        }
        else if (strcmp(parsed[0], "USE") == 0) {
            useDB(fd, parsed[1]);
        }
        else if (strcmp(parsed[0], "GRANT") == 0) {
            grantDB(fd, parsed[2], parsed[4]);
        }
        else write(fd, "Invalid query\n\n", SIZE_BUFFER);
    }
    if (fd == curr_fd) {
        curr_fd = curr_id = -1;
        memset(curr_db, '\0', sizeof(char) * DATA_BUFFER);
    }
    printf("Close connection with fd: %d\n", fd);
    close(fd);
}

/****   Controllers   *****/
void createTable(int fd, char parsed[20][DATA_BUFFER])
{
    if (strlen(curr_db) == 0) {
        write(fd, "Error::No database used\n\n", SIZE_BUFFER);
        return;
    }
    char *table = parsed[2];

    // Make sure that table doesn't exist in the current database
    FILE *fp = getTable(curr_db, table, "r", NULL);
    if (fp != NULL) {
        fclose(fp);
        write(fd, "Error::Table already exists\n\n", SIZE_BUFFER);
        return;
    }

    // Get collumns
    char cols[DATA_BUFFER];
    strcpy(cols, parsed[3] + 1);
    for (int i = 5; i < 20; i+=2) {
        if (strlen(parsed[i]) == 0) {
            break;
        }
        strcat(cols, ",");
        strcat(cols, parsed[i]);
    }

    fp = getTable(curr_db, table, "a", cols);
    fclose(fp);
    write(fd, "Table created\n\n", SIZE_BUFFER);
}

void createDB(int fd, char *db_name)
{
    if (dbExist(fd, db_name, false)) {
        write(fd, "Error::Database already exists\n\n", SIZE_BUFFER);
        return;
    }
    if (mkdir(db_name, 0777) == -1) {
        write(fd, 
            "Error::Unknown error occurred when creating new database\n\n", 
            SIZE_BUFFER);
        return;
    }
    if (curr_id != 0) {
        FILE *fp = getTable("config", "permissions", "a", "id,db_name");
        fprintf(fp, "%d,%s\n", curr_id, db_name);
        fclose(fp);
    }
    write(fd, "Database created\n\n", SIZE_BUFFER);
}

void grantDB(int fd, char *db_name, char *username)
{
    if (curr_id != 0 || strcmp(db_name, "config") == 0) {
        write(fd, "Error::Forbidden action\n\n", SIZE_BUFFER);
        return;
    }
    if (!dbExist(fd, db_name, true)) {
        return;
    }
    int target_id = getUserId(username, NULL);
    if (target_id == -1) {
        write(fd, "Error::User not found\n\n", SIZE_BUFFER);
        return;
    }
    bool alreadyExist = false;

    FILE *fp = getTable("config", "permissions", "r", "id,db_name");
    char db[DATA_BUFFER], input[DATA_BUFFER];
    sprintf(input, "%d,%s", target_id, db_name);
    while (fscanf(fp, "%s", db) != EOF) {
        if (strcmp(input, db) == 0) {
            alreadyExist = true;
            break;
        }
    }
    fclose(fp);

    if (alreadyExist) {
        write(fd, "Info::User already authorized\n\n", SIZE_BUFFER);
    } else {
        FILE *fp = getTable("config", "permissions", "a", NULL);
        fprintf(fp, "%d,%s\n", target_id, db_name);
        fclose(fp);
        write(fd, "Permission added\n\n", SIZE_BUFFER);
    }
}

void useDB(int fd, char *db_name)
{
    if (!dbExist(fd, db_name, true)) {
        return;
    }
    DIR *dp = opendir(db_name);
    bool authorized = false;

    if (curr_id != 0) {
        FILE *fp = getTable("config", "permissions", "r", NULL);
        if (fp != NULL) {
            char db[DATA_BUFFER], input[DATA_BUFFER];
            sprintf(input, "%d,%s", curr_id, db_name);
            while (fscanf(fp, "%s", db) != EOF) {
                if (strcmp(input, db) == 0) {
                    authorized = true;
                    break;
                }
            }
            fclose(fp);
        } else {
            authorized = false;
        }  
    } else {
        authorized = true;
    }

    if (authorized) {
        changeCurrDB(fd, db_name);
    } else {
        write(fd, "Error::Unauthorized access\n\n", SIZE_BUFFER);
    }
}

void regist(int fd, char *username, char *password)
{
    if (curr_id != 0) {
        write(fd, "Error::Forbidden action\n\n", SIZE_BUFFER);
        return;
    }

    FILE *fp = getTable("config", "users", "a", "id,username,password");
    int id = getUserId(username, password);

    if (id != -1) {
        write(fd, "Error::User is already registered\n\n", SIZE_BUFFER);
    } else {
        id = getLastId("config", "users") + 1;
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
        FILE *fp = getTable("config", "users", "r", NULL);
        if (fp != NULL) {
            id = getUserId(username, password);
            fclose(fp);
        }
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

/*****  SERVICES  *****/
void changeCurrDB(int fd, const char *db_name)
{
    strcpy(curr_db, db_name);
    write(fd, "change type", SIZE_BUFFER);
    write(fd, db_name, SIZE_BUFFER);
}

bool dbExist(int fd, char *db_name, bool printError)
{
    struct stat s;
    int err = stat(db_name, &s);
    if (err == -1 || !S_ISDIR(s.st_mode)) {
        if (printError) write(fd, "Error::Database not found\n\n", SIZE_BUFFER);
        return false;
    }
    return true;
}

int getUserId(char *username, char *password)
{
    int id = -1;
    FILE *fp = getTable("config", "users", "r", NULL);

    if (fp != NULL) {
        char db[DATA_BUFFER], input[DATA_BUFFER];
        if (password != NULL) {
            sprintf(input, "%s,%s", username, password);
        } else {
            sprintf(input, "%s", username);
        }
        
        while (fscanf(fp, "%s", db) != EOF) {
            char *temp = strstr(db, ",") + 1; // Get username and password from db
            
            if (password == NULL) {
                temp = strtok(temp, ",");
            }
            if (strcmp(temp, input) == 0) {
                id = atoi(strtok(db, ","));  // Get id from db
                break;
            }
        }
        fclose(fp);
    }
    return id;
}

int getLastId(char *db_name, char *table)
{
    int id = 1;
    FILE *fp = getTable(db_name, table, "r", NULL);

    if (fp != NULL) {
        char db[DATA_BUFFER];
        while (fscanf(fp, "%s", db) != EOF) {
            id = atoi(strtok(db, ","));  // Get id from db
        }
    }
    return id;
}

void explode(char string[], char storage[20][DATA_BUFFER], const char *delimiter)
{
    char *buf = string;
    char *temp = NULL;
    memset(storage, '\0', sizeof(char) * 20 * DATA_BUFFER);

    int i = 0;
    while ((temp = strtok(buf, delimiter)) != NULL && i < 20) {
        if (buf != NULL) {
            buf = NULL;
        }
        strcpy(storage[i++], temp);
    }

    // Remove ";""
    char *ptr = strchr(storage[--i], ';');
    if (ptr != NULL) {
        *ptr = '\0';
    }
}

FILE *getTable(char *db_name, char *table, char *cmd, char *collumns)
{
    char path[DATA_BUFFER];
    sprintf(path, "./%s/%s.csv", db_name, table);

    if (access(path, F_OK) != 0 && collumns != NULL) {
        FILE *fp = fopen(path, "w");
        fprintf(fp, "%s\n", collumns);
        fclose(fp);
    }
    return fopen(path, cmd);
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
