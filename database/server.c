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
char auth_user[2][DATA_BUFFER]; // [0] => id, [1] => pass
const int SIZE_BUFFER = sizeof(char) * DATA_BUFFER;

const char *currDir = "/home/frain8/Documents/Sisop/FP/database/databases";
const char *USERS_TABLE = "./config/users.csv";

// Socket setup
int create_tcp_server_socket();
int *makeDaemon(pid_t *pid, pid_t *sid);

// Routes & controller
void *routes(void *argv);
bool login(int fd, char *username, char *password); // adjusted
void regist(int fd);
void add(char *buf, int fd);
void download(char *filename, int fd);
void delete(char *filename, int fd);
void see(char *buf, int fd, bool isFind);
void _log(char *cmd, char *filepath);

// Helper
int getInput(int fd, char *prompt, char *storage);
int getCredentials(int fd, char *id, char *password);
int writeFile(int fd, char *dirname, char *targetFileName);
int sendFile(int fd, char *filename);
char *getFileName(char *filePath);
bool isRegistered(FILE *fp, char *id);
bool alreadyDownloaded(FILE *fp, char *filename);
void parseFilePath(char *filepath, char *raw_filename, char *ext);

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

    while (read(fd, query, DATA_BUFFER) != 0) {
        strcpy(buf, query);
        char *cmd = strtok(buf, " ");

        if (strcmp(cmd, "login") == 0) {
            char *username = strtok(NULL, " ");
            char *password = (strcmp(username, "root") != 0) 
                            ? strtok(NULL, " ") : "root";
            if (!login(fd, username, password))
                break;
        }

        sleep(0.001);
    }
    if (fd == curr_fd) {
        curr_fd = curr_id = -1;
    }
    printf("Close connection with fd: %d\n", fd);
    close(fd);
}

/****   Controllers   *****/
void see(char *buf, int fd, bool isFind)
{
    int counter = 0;
    FILE *src = fopen("files.tsv", "r");
    if (!src) {
        write(fd, "Files.tsv not found\n", SIZE_BUFFER);
        return;
    }

    char temp[DATA_BUFFER + 85], raw_filename[DATA_BUFFER/3], ext[5],
        filepath[DATA_BUFFER/3], publisher[DATA_BUFFER/3], year[10];
        
    while (fscanf(src, "%s\t%s\t%s", filepath, publisher, year) != EOF) {
        parseFilePath(filepath, raw_filename, ext);
        if (isFind && strstr(raw_filename, buf) == NULL) continue;
        counter++;

        sprintf(temp, 
            "Nama: %s\nPublisher: %s\nTahun publishing: %s\nEkstensi File: %s\nFilepath: %s\n\n",
            raw_filename, publisher, year, ext, filepath
        );
        write(fd, temp, SIZE_BUFFER);
        sleep(0.001);
    }
    if(counter == 0) {
        if (isFind) write(fd, "Query not found in files.tsv\n", SIZE_BUFFER);
        else write(fd, "Empty files.tsv\n", SIZE_BUFFER);
    } 
    fclose(src);
}

void delete(char *filename, int fd)
{
    // buf is the deleted filename
    FILE *fp = fopen("files.tsv", "a+");
    char db[DATA_BUFFER], currFilePath[DATA_BUFFER], publisher[DATA_BUFFER], year[DATA_BUFFER];

    if (alreadyDownloaded(fp, filename)) {
        rewind(fp);
        FILE *tmp_fp = fopen("temp.tsv", "a+");

        // Copy files.tsv to temp
        while (fgets(db, SIZE_BUFFER, fp)) {
            sscanf(db, "%s\t%s\t%s", currFilePath, publisher, year);
            if (strcmp(getFileName(currFilePath), filename) != 0) { // Skip file with name equal to buf variable
                fprintf(tmp_fp, "%s", db);
            }
            memset(db, 0, SIZE_BUFFER);
        }
        fclose(tmp_fp);
        fclose(fp);
        remove("files.tsv");
        rename("temp.tsv", "files.tsv");

        char deletedFileName[DATA_BUFFER];
        sprintf(deletedFileName, "FILES/%s", filename);

        char newFileName[DATA_BUFFER];
        sprintf(newFileName, "FILES/old-%s", filename);

        rename(deletedFileName, newFileName);
        send(fd, "Delete file success\n", SIZE_BUFFER, 0);
        _log("delete", filename);
    } 
    else {
        send(fd, "Error: File hasn't been downloaded\n", SIZE_BUFFER, 0);
        fclose(fp);
    }
}

void download(char *filename, int fd)
{
    FILE *fp = fopen("files.tsv", "a+");
    if (alreadyDownloaded(fp, filename)) {
        sendFile(fd, filename);
    } else {
        send(fd, "Error: File hasn't been downloaded\n", SIZE_BUFFER, 0);
    }
    fclose(fp);
}

void add(char *buf, int fd)
{
    char *dirName = "FILES";
    char publisher[DATA_BUFFER], year[DATA_BUFFER], client_path[DATA_BUFFER];
    sleep(0.001);
    if (getInput(fd, "Publisher: ", publisher) == 0) return;
    if (getInput(fd, "Tahun Publikasi: ", year) == 0) return;
    if (getInput(fd, "Filepath: ", client_path) == 0) return;

    FILE *fp = fopen("files.tsv", "a+");
    char *fileName = getFileName(client_path);

    if (alreadyDownloaded(fp, fileName)) {
        send(fd, "Error: File is already uploaded\n", SIZE_BUFFER, 0);
    } else {
        send(fd, "Start sending file\n", SIZE_BUFFER, 0);
        mkdir(dirName, 0777);
        if (writeFile(fd, dirName, fileName) == 0) {
            fprintf(fp, "%s/%s\t%s\t%s\n", dirName, fileName, publisher, year);
            printf("Store file finished\n");
            _log("add", fileName);
        } else {
            printf("Error occured when receiving file\n");
        }
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
    } else {
        FILE *fp = fopen(USERS_TABLE, "r");
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

void regist(int fd)
{
    char id[DATA_BUFFER], password[DATA_BUFFER];
    FILE *fp = fopen(USERS_TABLE, "a+");

    if (getCredentials(fd, id, password) != 0) {
        if (isRegistered(fp, id)) {
            send(fd, "Id is already registered\n", SIZE_BUFFER, 0);
        } else {
            fprintf(fp, "%s:%s\n", id, password);
            send(fd, "Register success\n", SIZE_BUFFER, 0);
        }
    }
    fclose(fp);
}

/*****  HELPER  *****/
void _log(char *cmd, char *filename)
{
    FILE *fp = fopen("running.log", "a+");
    cmd = (strcmp(cmd, "add") == 0) ? "Tambah" : "Hapus";
    fprintf(fp, "%s : %s (%s:%s)\n", cmd, filename, auth_user[0], auth_user[1]);
    fclose(fp);
}

void parseFilePath(char *filepath, char *raw_filename, char *ext)
{
    char *temp;
    if (temp = strrchr(filepath, '.')) strcpy(ext, temp + 1);
    else strcpy(ext, "-");

    strcpy(raw_filename, getFileName(filepath));
    strtok(raw_filename, ".");
}

int sendFile(int fd, char *filename)
{
    char buf[DATA_BUFFER] = {0};
    int ret_val;
    printf("Sending [%s] file to client!\n", filename);
    strcpy(buf, filename);
    sprintf(filename, "FILES/%s", buf);
    FILE *fp = fopen(filename, "r");

    if (!fp) {
        printf("File not found\n");
        send(fd, "File not found\n", SIZE_BUFFER, 0);
        return -1;
    }
    send(fd, "Start receiving file\n", SIZE_BUFFER, 0);
    send(fd, buf, SIZE_BUFFER, 0);

    // Transfer size
    fseek(fp, 0L, SEEK_END);
    int size = ftell(fp);
    rewind(fp);
    sprintf(buf, "%d", size);
    send(fd, buf, SIZE_BUFFER, 0);

    while ((ret_val = fread(buf, 1, DATA_BUFFER, fp)) > 0) {
        send(fd, buf, ret_val, 0);
    }
    recv(fd, buf, DATA_BUFFER, 0);
    printf("Send file finished\n");
    fclose(fp);
    return 0;
}

char *getFileName(char *filePath)
{
    char *ret = strrchr(filePath, '/');
    if (ret) return ret + 1;
    else return filePath;
}

int writeFile(int fd, char *dirname, char *targetFileName)
{
    int ret_val, size;
    char buf[DATA_BUFFER] = {0};
    char in[1];

    // Make sure that client has the file
    ret_val = recv(fd, buf, DATA_BUFFER, 0);
    if (ret_val == 0 || strcmp(buf, "File found") != 0) {
        if (ret_val == 0) printf("Connection to client lost\n");
        else puts(buf);
        return -1;
    }
    recv(fd, buf, SIZE_BUFFER, 0);
    size = atoi(buf);

    printf("Store [%s] file from client\n", targetFileName);
    sprintf(buf, "%s/%s", dirname,targetFileName);
    FILE *fp = fopen(buf, "w+");

    while (size-- > 0) {
        if ((ret_val = recv(fd, in, 1, 0)) < 0)
            return ret_val;
        fwrite(in, 1, 1, fp);
    }
    ret_val = 0;
    printf("Storing file finished\n");
    fclose(fp);
    return ret_val;
}

int getCredentials(int fd, char *id, char *password)
{
    if (getInput(fd, "Insert id: ", id) == 0) return 0;
    if (getInput(fd, "Insert password: ", password) == 0) return 0;
    return 1;
}

int getInput(int fd, char *prompt, char *storage)
{
    send(fd, prompt, SIZE_BUFFER, 0);

    // Get input
    int ret_val = recv(fd, storage, DATA_BUFFER, 0);
    if (ret_val != 0) printf("Input: [%s]\n", storage);
    return ret_val;
}

bool isRegistered(FILE *fp, char *id)
{
    char db[DATA_BUFFER], *tmp;
    while (fscanf(fp, "%s", db) != EOF) {
        tmp = strtok(db, ":");
        if (strcmp(tmp, id) == 0) return true;
    }
    return false;
}

bool alreadyDownloaded(FILE *fp, char *filename)
{
    char db[DATA_BUFFER], *tmp;
    while (fscanf(fp, "%s", db) != EOF) {
        tmp = getFileName(strtok(db, "\t"));
        if (strcmp(tmp, filename) == 0) return true;
    }
    return false;
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
