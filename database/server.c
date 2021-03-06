#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <strings.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <ftw.h>

#define DATA_BUFFER 150
#define SMALL 40

int curr_fd = -1;
int curr_id = -1;
char curr_db[SMALL] = {0};

const int SIZE_BUFFER = sizeof(char) * DATA_BUFFER;
const char *currDir = "/home/frain8/Documents/Sisop/FP/database/databases"; 

// Socket setup
int create_tcp_server_socket();
void makeDaemon(pid_t *pid, pid_t *sid);

// Routes & controller
void *routes(void *argv);
bool login(int fd, char *username, char *password);
void regist(int fd, char *username, char *password);
void useDB(int fd, char *db_name);
void grantDB(int fd, char *db_name, char *username);
void createDB(int fd, char *db_name);
void createTable(int fd, char parsed[20][SMALL]);
void dropDB(int fd, char *db_name);
void dropTable(int fd, char *table);
void dropColumn(int fd, char *col, char *table);
void deleteTable(int fd, char *table, char *col_val);
void insertTable(int fd, char *table, char *input);

// Services
int getInput(int fd, char *prompt, char *storage);
int getUserId(char *username, char *password);
int getLastId(char *db_name, char *table);
void explode(char *string, char storage[20][SMALL], const char *delimiter);
void changeCurrDB(int fd, const char *db_name);
FILE *getTable(char *db_name, char *table, char *cmd); // If table not exist, return NULL
FILE *getOrMakeTable(char *db_name, char *table, char *cmd, char *collumns); // If table not exist, make new table
bool dbExist(int fd, char *db_name, bool printError);
bool tableExist(int fd, char *db_name, char *table, bool printError);
bool canAccessDB(int fd, int user_id, char *db_name, bool printError);
int deleteDB(char *db_name);
int _deleteDB(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
bool _deleteTable(int fd, char *db_name, char *table, char *col, char *val, bool printSuccess);
void logging(const char *username, const char *query);
void removeTick(char str[]);

int main()
{
    // TODO:: uncomment on final
    // pid_t pid, sid;
    // makeDaemon(&pid, &sid);

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
    chdir(currDir);
    int fd = *(int *) argv;
    bool logged_in = false;
    char username[SMALL];
    char query[DATA_BUFFER], buf[DATA_BUFFER];
    char parsed[20][SMALL];

    while (read(fd, query, DATA_BUFFER) != 0) {
        // puts(query); // TODO::Comment on final
        if (logged_in && query[0] != '\0') {
            logging(username, query);
        }
        explode(query, parsed, " ");

        if (strcmp(parsed[0], "LOGIN") == 0) {
            if (login(fd, parsed[1], parsed[2])) {
                strcpy(username, parsed[1]);
                logged_in = true;
            } 
            else break;
        }
        else if (strcmp(parsed[0], "USE") == 0) {
            useDB(fd, parsed[1]);
        }
        else if (strcmp(parsed[0], "GRANT") == 0) {
            grantDB(fd, parsed[2], parsed[4]);
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
            else write(fd, "Invalid query on CREATE command", SIZE_BUFFER);
        }
        else if (strcmp(parsed[0], "DROP") == 0) {
            if (strcmp(parsed[1], "DATABASE") == 0) {
                dropDB(fd, parsed[2]);
            }
            else if (curr_db[0] != '\0') {
                if (strcmp(parsed[1], "TABLE") == 0) {
                    dropTable(fd, parsed[2]);
                }
                else if (strcmp(parsed[1], "COLUMN") == 0) {
                    dropColumn(fd, parsed[2], parsed[4]);
                }
                else write(fd, "Invalid query on DROP command", SIZE_BUFFER);
            }
            else write(fd, "Error::No database used", SIZE_BUFFER);
        }
        else if (strcmp(parsed[0], "DELETE") == 0 
                && strcmp(parsed[1], "FROM") == 0) {
            deleteTable(fd, parsed[2], parsed[4]);
        }
        else if (strcmp(parsed[0], "INSERT") == 0) {
            insertTable(fd, parsed[2], parsed[3]);
        }
        else write(fd, "Invalid query", SIZE_BUFFER);
    }
    if (fd == curr_fd) {
        curr_fd = curr_id = -1;
        bzero(curr_db, SIZE_BUFFER);
    }
    printf("Close connection with fd: %d\n", fd);
    close(fd);
}


/****   Controllers   *****/
void insertTable(int fd, char *table, char *input)
{
    if (curr_db[0] == '\0') {
        write(fd, "Error::No database used", SIZE_BUFFER);
        return;
    }
    if (!tableExist(fd, curr_db, table, true)) {
        return;
    }
    char parsed[20][SMALL] = {0};
    explode(input, parsed, ",");

    // Get first row
    char db[DATA_BUFFER];
    FILE *fp = getTable(curr_db, table, "r");
    fscanf(fp, "%s", db);
    fclose(fp);

    // Make sure that input doesn't exceed the number of column on db
    int db_col = 1;
    for (int i = strlen(db) - 1; i >= 0; i--) if (db[i] == ',') db_col++;
    if (parsed[db_col - 1][0] == '\0') {
        write(fd, "Error::Lacking column on insertion", SIZE_BUFFER);
        return;
    }
    if (db_col >= 20 || parsed[db_col][0] != '\0') {
        write(fd, "Error::Too many column on insertion", SIZE_BUFFER);
        return;
    }

    // Get collumns
    char cols[DATA_BUFFER] = {0};
    for (int i = 0; i < 20; i++) {
        if (parsed[i][0] == '\0')
            break;

        // Clean value from "'", "(", and ")"
        if (parsed[i][0] == '(') {
            strcpy(parsed[i], parsed[i] + 1);
        } 
        int last_char = strlen(parsed[i]) - 1;
        if (parsed[i][last_char] == ')') {
            parsed[i][last_char--] = '\0';
        }
        if (parsed[i][last_char] == '\'') {
            removeTick(parsed[i]);
            last_char -= 2;
        }
        if (i != 0) strcat(cols, ",");
        strcat(cols, parsed[i]);
    }
    fp = getTable(curr_db, table, "a");
    fprintf(fp, "%s\n", cols);
    fclose(fp);
    write(fd, "Insert success", SIZE_BUFFER);
}

void deleteTable(int fd, char *table, char *col_val)
{
    if (curr_db[0] == '\0') {
        write(fd, "Error::No database used", SIZE_BUFFER);
        return;
    }
    if (!tableExist(fd, curr_db, table, true)) {
        return;
    }
    if (col_val == NULL || col_val[0] == '\0') { // Delete table
        _deleteTable(fd, curr_db, table, NULL, NULL, true);
    } 
    else { // Delete specific row
        char parsed[2][SMALL] = {0}; // 0 => column name, 1 => value
        strcpy(parsed[0], strtok(col_val, "="));
        strcpy(parsed[1], strtok(NULL, "="));
        removeTick(parsed[1]);
        _deleteTable(fd, curr_db, table, parsed[0], parsed[1], true);
    }
}

void dropColumn(int fd, char *col, char *table)
{
    if (tableExist(fd, curr_db, table, true)) {
        bool isDeleted = _deleteTable(fd, curr_db, table, col, NULL, false);
        if (isDeleted) {
            write(fd, "Column dropped", SIZE_BUFFER);
        }
    }
}

void dropTable(int fd, char *table)
{
    if (tableExist(fd, curr_db, table, true)) {
        char path[DATA_BUFFER];
        sprintf(path, "./%s/%s.csv", curr_db, table);
        remove(path);
        write(fd, "Table dropped", SIZE_BUFFER);
    }
}

void dropDB(int fd, char *db_name)
{
    if (strcmp(db_name, "config") == 0) {
        write(fd, "Error:Can't drop configuration database", SIZE_BUFFER);
        return;
    }
    if (!canAccessDB(fd, curr_id, db_name, true)) {
        return;
    }
    if (deleteDB(db_name) == -1) {
        write(fd, "Error:Unknown error occured when deleting database", SIZE_BUFFER);
        return;
    }
    if (strcmp(curr_db, db_name) == 0) {
        write(fd, ">Wait", SIZE_BUFFER);
        changeCurrDB(fd, NULL);
    }
    bool isDeleted = _deleteTable(fd, "config", "permissions", "db_name", db_name, false);
    if (isDeleted) write(fd, "Database dropped", SIZE_BUFFER);
}

void createTable(int fd, char parsed[20][SMALL])
{
    if (curr_db[0] == '\0') {
        write(fd, "Error::No database used", SIZE_BUFFER);
        return;
    }
    char *table = parsed[2];

    // Make sure that table doesn't exist in the current database
    if (tableExist(fd, curr_db, table, false)) {
        write(fd, "Error::Table already exists", SIZE_BUFFER);
        return;
    }

    // Get collumns
    char cols[DATA_BUFFER];
    strcpy(cols, parsed[3] + 1);
    for (int i = 5; i < 20; i+=2) {
        if (parsed[i][0] == '\0') {
            break;
        }
        strcat(cols, ",");
        strcat(cols, parsed[i]);
    }

    FILE *fp = getOrMakeTable(curr_db, table, "a", cols);
    fclose(fp);
    write(fd, "Table created", SIZE_BUFFER);
}

void createDB(int fd, char *db_name)
{
    if (dbExist(fd, db_name, false)) {
        write(fd, "Error::Database already exists", SIZE_BUFFER);
    }
    else if (strstr(db_name, "new-") == db_name) { // Can't make dir that start with prefix "new-""
        write(fd, "Error::Cannot create database with prefix \"new-\"", SIZE_BUFFER);
    }
    else if (mkdir(db_name, 0777) == -1) {
        write(fd, 
            "Error::Unknown error occurred when creating new database", 
            SIZE_BUFFER);
    } else {
        if (curr_id != 0) {
            FILE *fp = getOrMakeTable("config", "permissions", "a", "id,username,password");
            fprintf(fp, "%d,%s\n", curr_id, db_name);
            fclose(fp);
        }
        write(fd, "Database created", SIZE_BUFFER);
    }
}

void grantDB(int fd, char *db_name, char *username)
{
    if (curr_id != 0 || strcmp(db_name, "config") == 0) {
        write(fd, "Error::Forbidden action", SIZE_BUFFER);
        return;
    }
    if (!dbExist(fd, db_name, true)) {
        return;
    }
    int target_id = getUserId(username, NULL);
    if (target_id == -1) {
        write(fd, "Error::User not found", SIZE_BUFFER);
        return;
    }
    bool alreadyExist = false;
    char *cols = NULL;

    FILE *fp = getOrMakeTable("config", "permissions", "r", "id,username,password");
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
        write(fd, "Info::User already authorized", SIZE_BUFFER);
    } else {
        FILE *fp = getTable("config", "permissions", "a");
        fprintf(fp, "%d,%s\n", target_id, db_name);
        fclose(fp);
        write(fd, "Permission added", SIZE_BUFFER);
    }
}

void useDB(int fd, char *db_name)
{
    if (canAccessDB(fd, curr_id, db_name, true)) {
        changeCurrDB(fd, db_name);
    }
}

void regist(int fd, char *username, char *password)
{
    char *cols = NULL;
    if (curr_id != 0) {
        write(fd, "Error::Forbidden action", SIZE_BUFFER);
        return;
    }
    FILE *fp = getOrMakeTable("config", "users", "a", "id,username,password");
    int id = getUserId(username, password);

    if (id != -1) {
        write(fd, "Error::User is already registered", SIZE_BUFFER);
    } else {
        id = getLastId("config", "users") + 1;
        fprintf(fp, "%d,%s,%s\n", id, username, password);
        write(fd, "Register success", SIZE_BUFFER);
    }
    fclose(fp);
}

bool login(int fd, char *username, char *password)
{
    if (curr_fd != -1) {
        write(fd, "Server is busy, wait for other user to logout.", SIZE_BUFFER);
        return false;
    }

    int id;
    if (strcmp(username, "root") == 0) {
        id = 0;
    } else {
        id = getUserId(username, password); // Check data in DB
    }

    if (id == -1) {
        write(fd, "Error::Invalid id or password", SIZE_BUFFER);
        return false;
    } else {
        write(fd, "Login success", SIZE_BUFFER);
        curr_fd = fd;
        curr_id = id;
    }
    return true;
}

void logging(const char *username, const char *query)
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    char time[SMALL], date[SMALL];
    sprintf(time, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    sprintf(date, "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon, tm.tm_mday);

    char output[DATA_BUFFER];
    sprintf(output, "%s %s:%s:%s", date, time, username, query);

    // Remove ";"
    int last_char = strlen(output) - 1;
    if (output[last_char] == ';') {
        output[last_char] = '\0';
    }

    FILE *F_out = fopen("logging.log", "a+");
    fprintf(F_out, "%s\n", output);
    fclose(F_out);
}

/*****  SERVICES  *****/
void removeTick(char str[])
{
    int last_char = strlen(str) - 1;
    if (str[last_char] == '\'') {
        str[last_char] = '\0';
        char temp[SMALL];
        strcpy(temp, str);
        strcpy(str, temp + 1);
    }
}

bool _deleteTable(int fd, char *db_name, char *table, char *col, char *val, bool printSuccess)
{
    if (db_name == NULL) {
        write(fd, "Error::No database specified on delete", SIZE_BUFFER);
        return false;
    }
    
    // Delete table
    if (col == NULL && val == NULL) {
        char first_row[DATA_BUFFER];
        FILE *fp = getTable(db_name, table, "r");
        fscanf(fp, "%s", first_row);
        fclose(fp);

        fp = getTable(db_name, table, "w");
        fprintf(fp, "%s\n", first_row);
        fclose(fp);
        if (printSuccess) write(fd, "Table deleted", SIZE_BUFFER);
        return true;
    }

    // Get col index in db
    int col_index = -1;
    int last_index = -1;
    char db[DATA_BUFFER], parsed[20][SMALL];
    FILE *fp = getTable(db_name, table, "r");
    fscanf(fp, "%s", db);
    rewind(fp);
    explode(db, parsed, ",");
    for (int i = 0; i < 20; i++) {
        if (parsed[i][0] == '\0') {
            last_index = --i;
            break;
        }
        if (strcmp(parsed[i], col) == 0) {
            col_index = i;
        }
    }
    if (col_index == -1 || last_index == -1) {
        write(fd, "Error::Collumn not found or schema isn't defined", SIZE_BUFFER);
        return false;
    }

    char new_table[SMALL];
    sprintf(new_table, "new-%s", table);
    FILE *new_fp = getOrMakeTable(db_name, new_table, "w", NULL);

    // Delete collumn
    if (val == NULL) { 
        while (fscanf(fp, "%s", db) != EOF) {
            explode(db, parsed, ",");
            for (int i = 0; i <= last_index; i++) {
                if (i != col_index) {
                    fprintf(new_fp, "%s", parsed[i]);
                    if (i != last_index) fprintf(new_fp, ",");
                }
            }
            fprintf(new_fp, "\n");
        }
        if (printSuccess) write(fd, "Collumn dropped", SIZE_BUFFER);
    }
    else { // Delete specific collumn
        int counter  = 0;
        while (fscanf(fp, "%s", db) != EOF) {
            explode(db, parsed, ",");
            if (strcmp(val, parsed[col_index]) != 0) {
                fprintf(new_fp, "%s\n", db);
            } else {
                counter++;
            }
        }
        if (printSuccess) {
            sprintf(db, "Delete success, %d row has been deleted", counter);
            write(fd, db, SIZE_BUFFER);
        }
    }
    fclose(new_fp);
    fclose(fp);

    // Swap file
    char path[DATA_BUFFER], new_path[DATA_BUFFER];
    sprintf(path, "./%s/%s.csv", db_name, table);
    sprintf(new_path, "./%s/%s.csv", db_name, new_table);
    remove(path);
    rename(new_path, path);
    return true;
}

int deleteDB(char *db_name)
{
    return nftw(db_name, _deleteDB, 64, FTW_DEPTH | FTW_PHYS);
}

int _deleteDB(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    int rv = remove(fpath);
    if (rv) perror(fpath);
    return rv;
}

bool canAccessDB(int fd, int user_id, char *db_name, bool printError)
{
    if (!dbExist(fd, db_name, printError)) {
        return false;
    }

    bool authorized = false;
    if (user_id != 0 && tableExist(fd, "config", "permissions", false)) {
        FILE *fp = getTable("config", "permissions", "r");
        if (fp !=  NULL) {
            char db[DATA_BUFFER], input[DATA_BUFFER];
            sprintf(input, "%d,%s", user_id, db_name);
            while (fscanf(fp, "%s", db) != EOF) {
                if (strcmp(input, db) == 0) {
                    authorized = true;
                    break;
                }
            }
            fclose(fp);
        }
    } else {
        authorized = true;
    }
    if (!authorized && printError) {
        write(fd, "Error::Unauthorized access", SIZE_BUFFER);
    }
    return authorized;
}

void changeCurrDB(int fd, const char *db_name)
{
    write(fd, ">Change type", SIZE_BUFFER);
    if (db_name == NULL) {
        bzero(curr_db, SIZE_BUFFER);
        write(fd, (curr_id == 0) ? "root" : "user", SIZE_BUFFER);
    } 
    else {
        strcpy(curr_db, db_name);
        write(fd, db_name, SIZE_BUFFER);
    }
}

bool dbExist(int fd, char *db_name, bool printError)
{
    struct stat s;
    int err = stat(db_name, &s);
    if (err == -1 || !S_ISDIR(s.st_mode)) {
        if (printError) write(fd, "Error::Database not found", SIZE_BUFFER);
        return false;
    }
    return true;
}

bool tableExist(int fd, char *db_name, char *table, bool printError)
{
    struct stat s;
    char path[DATA_BUFFER];
    sprintf(path, "./%s/%s.csv", db_name, table);
    int err = stat(path, &s);
    if (err == -1 || !S_ISREG(s.st_mode)) {
        if (printError && fd != -1) write(fd, "Error::Table not found", SIZE_BUFFER);
        return false;
    }
    return true;
}

int getUserId(char *username, char *password)
{
    int id = -1;
    FILE *fp = getTable("config", "users", "r");
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
    FILE *fp = getTable(db_name, table, "r");
    if (fp != NULL) {
        char db[DATA_BUFFER];
        while (fscanf(fp, "%s", db) != EOF) {
            id = atoi(strtok(db, ","));  // Get id from db
        }
        fclose(fp);
    }
    return id;
}

void explode(char string[], char storage[20][SMALL], const char *delimiter)
{
    char _buf[DATA_BUFFER];
    strcpy(_buf, string);
    char *buf = _buf;
    char *temp = NULL;
    bzero(storage, sizeof(char) * 20 * SMALL);

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

// If table not exist, return NULL
FILE *getTable(char *db_name, char *table, char *cmd)
{
    FILE *fp = NULL;
    if (tableExist(-1, db_name, table, false)) {
        char path[DATA_BUFFER];
        sprintf(path, "./%s/%s.csv", db_name, table);
        fp = fopen(path, cmd);
    }
    return fp;
}

// If table not exist, make new table
FILE *getOrMakeTable(char *db_name, char *table, char *cmd, char *collumns)
{
    FILE *fp = getTable(db_name, table, cmd);
    if (fp == NULL) {
        char path[DATA_BUFFER];
        sprintf(path, "./%s/%s.csv", db_name, table);

        FILE *_fp = fopen(path, "w");
        if (collumns != NULL) fprintf(_fp, "%s\n", collumns);
        fclose(_fp);
        fp = fopen(path, cmd);
    }
    return fp;
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
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    // printf("Created a socket with fd: %d\n", fd); // TODO::Comment on final

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

void makeDaemon(pid_t *pid, pid_t *sid)
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
}
