#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>

#define DATA_BUFFER 300

const int SIZE_BUFFER = sizeof(char) * DATA_BUFFER;
char inputPath[DATA_BUFFER];
bool _inputPath = false;

int create_tcp_client_socket();
void *handleInput(void *client_fd);
void *handleOutput(void *client_fd);
void getServerInput(int fd, char *input);
void sendFile(int fd);
void writeFile(int fd);

int main()
{
    pthread_t tid[2];
    int client_fd = create_tcp_client_socket();

    pthread_create(&(tid[0]), NULL, &handleOutput, (void *) &client_fd);
    pthread_create(&(tid[1]), NULL, &handleInput, (void *) &client_fd);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);

    close(client_fd);
    return 0;
}

void *handleInput(void *client_fd)
{
    int fd = *(int *) client_fd;
    char message[DATA_BUFFER] = {0};

    while (1) {
        gets(message);
        send(fd, message, SIZE_BUFFER, 0);
        if (_inputPath) {
            strcpy(inputPath, message);
        }
    }
}

void *handleOutput(void *client_fd) 
{
    int fd = *(int *) client_fd;
    char message[DATA_BUFFER] = {0};

    while (1) {
        memset(message, 0, SIZE_BUFFER);
        getServerInput(fd, message);
        printf("%s", message);
        
        if (strcmp(message, "Filepath: ") == 0) {
            _inputPath = true;
        } else if (strcmp(message, "Start sending file\n") == 0) {
            sendFile(fd);
            _inputPath = false;
        } else if (strcmp(message, "Error: file is already uploaded\n") == 0) {
            _inputPath = false;
        } else if (strcmp(message, "Start receiving file\n") == 0) {
            writeFile(fd);
        } 
        fflush(stdout);
    }
}

void sendFile(int fd)
{
    printf("Sending [%s] file to server!\n", inputPath);
    int ret_val;
    FILE *fp = fopen(inputPath, "r");
    char buf[DATA_BUFFER] = {0};

    if (fp) {
        send(fd, "File found", SIZE_BUFFER, 0);

        fseek(fp, 0L, SEEK_END);
        int size = ftell(fp);
        rewind(fp);
        sprintf(buf, "%d", size);
        send(fd, buf, SIZE_BUFFER, 0);

        while ((ret_val = fread(buf, 1, 1, fp)) > 0) {
            send(fd, buf, 1, 0);
        } 
        printf("Sending file finished!\n");
        fclose(fp);
    } else {
        printf("File not found\n");
        send(fd, "File not found", SIZE_BUFFER, 0);
    }
}

void writeFile(int fd)
{
    char buf[DATA_BUFFER] = {0};
    int ret_val = recv(fd, buf, DATA_BUFFER, 0);
    FILE *fp = fopen(buf, "w+");

    recv(fd, buf, DATA_BUFFER, 0);
    int size = atoi(buf);
    
    while (size > 0) {
        ret_val = recv(fd, buf, DATA_BUFFER, 0);
        fwrite(buf, 1, ret_val, fp);
        memset(buf, 0, SIZE_BUFFER);
        size -= ret_val;
    }
    puts("Send file finished");
    send(fd, "Send file finished", SIZE_BUFFER, 0);
    fclose(fp);
}

void getServerInput(int fd, char *input)
{
    if (recv(fd, input, DATA_BUFFER, 0) == 0) {
        printf("Server shutdown\n");
        exit(EXIT_SUCCESS);
    }
}

int create_tcp_client_socket()
{
    struct sockaddr_in saddr;
    int fd, ret_val;
    int opt = 1;
    struct hostent *local_host; /* need netdb.h for this */

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

    /* Let us initialize the server address structure */
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(7000);
    local_host = gethostbyname("127.0.0.1");
    saddr.sin_addr = *((struct in_addr *)local_host->h_addr);

    /* Step2: connect to the TCP server socket */
    ret_val = connect(fd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (ret_val == -1) {
        fprintf(stderr, "connect failed [%s]\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return fd;
}
