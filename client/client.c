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

#define DATA_BUFFER 150

const int SIZE_BUFFER = sizeof(char) * DATA_BUFFER;
char username[DATA_BUFFER] = {0};
char type[DATA_BUFFER] = {0};
bool wait = false;

// SETUP
int create_tcp_client_socket();
void *handleInput(void *client_fd);
void *handleOutput(void *client_fd);
void getServerOutput(int fd, char *input);

// Controller
bool login(int, int, char *[]);

// Helpet
bool isValid(char *);

int main(int argc, char *argv[])
{
    pthread_t tid[2];
    int client_fd = create_tcp_client_socket();

    if (!login(client_fd, argc, argv)) {
        return -1;
    }
    
    pthread_create(&(tid[0]), NULL, &handleOutput, (void *) &client_fd);
    pthread_create(&(tid[1]), NULL, &handleInput, (void *) &client_fd);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);

    close(client_fd);
    return 0;
}

/**    CONTROLLER    **/
bool login(int fd, int argc, char *argv[])
{
    char buf[DATA_BUFFER];
    if (geteuid() == 0) { // root
        write(fd, "LOGIN root root", SIZE_BUFFER);
        // puts("LOGIN root root"); // TODO:: Delete on final

        strcpy(username, "root");
        strcpy(type, "root");
    } 
    else if (argc == 5
        && strcmp(argv[1] , "-u") == 0
        && strcmp(argv[3] , "-p") == 0
    ) { // user
        sprintf(buf, "LOGIN %s %s", argv[2], argv[4]);
        write(fd, buf, SIZE_BUFFER);
        // puts(buf); // TODO:: Delete on final

        strcpy(username, argv[2]);
        strcpy(type, "user");
    } 
    else {
        puts("Error::Invalid argument");
        return false;
    }
    read(fd, buf, SIZE_BUFFER);
    puts(buf);
    return strcmp(buf, "Login success\n") == 0;
}

bool isValid(char *message)
{
    /*
    * Cek:
    * 1. Jumlah kata
    * 2. Format perintah
    * 3. Tidak ada kata yang NULL
    * 4. Diakhiri dengan titik koma (;)
    * 5. Atur authorisasi (akses) dari akun user dan root
    * 
    * Jika return bernilai false, message tidak dikirim ke server
    */
    if (strcmp(message, "quit") == 0) {
        puts("Good bye :3");
        exit(EXIT_SUCCESS);
    }
    else {
        puts("Invalid query");
        return false;
    }
    return true;
}

/**    SETUP    **/
void *handleInput(void *client_fd)
{
    int fd = *(int *) client_fd;
    char message[DATA_BUFFER] = {0};

    while (1) {
        if (wait) {
            sleep(2); //TODO::Comment on final
            continue;
        };
        printf("%s@%s: ", username, type);
        fgets(message, DATA_BUFFER, stdin);

        // Remove trailing \n
        char *tmp = strtok(message, "\n");
        if (tmp != NULL) {
            strcpy(message, tmp);
        }
        if (strcasecmp(message, "quit") == 0) {
            puts("Good bye :3");
            exit(EXIT_SUCCESS);
        }
        
        // if (isValid(message)) { // TODO::Uncomment on final
            send(fd, message, SIZE_BUFFER, 0);
        // }
        wait = true;
    }
}

void *handleOutput(void *client_fd) 
{
    int fd = *(int *) client_fd;
    int counter = 0;
    char message[DATA_BUFFER] = {0};

    while (1) {
        memset(message, 0, SIZE_BUFFER);
        getServerOutput(fd, message);
        if (strcmp(message, "change type") == 0) {
            getServerOutput(fd, message);
            strcpy(type, message);
        }
        else if (strcmp(message, "Wait") == 0) {
            counter += 1;
        }
        else {
            printf("%s", message);
            fflush(stdout);
        }
        if (counter == 0) wait = false;
        else counter--;
    }
}

void getServerOutput(int fd, char *input)
{
    if (recv(fd, input, DATA_BUFFER, 0) == 0) {
        printf("Disconnected from server\n");
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
