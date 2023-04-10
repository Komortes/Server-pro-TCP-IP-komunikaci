#include <cstdlib>
#include <cstdio>
#include <sys/socket.h> // socket(), bind(), connect(), listen()
#include <unistd.h> // close(), read(), write()
#include <netinet/in.h> // struct sockaddr_in
#include <strings.h> // bzero()
#include <wait.h> // waitpid()
#include <iostream>
using namespace std;

constexpr int TIMEOUT = 10;
constexpr int BUFFER_SIZE = 1024;

int main(int argc, char **argv){
    int server, new_client_socket, port;
    char buffer[BUFFER_SIZE] = {0};
    if(argc < 2){ return cerr << "Usage: server port" << endl , -1;}

     // Создание сокета
    if ((server = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[1]);
    if(port == 0){
        cerr << "Usage: server port" << endl;
        close(server);
        return -1;
    }
   
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    int addrlen = sizeof(address);

    if (bind(server, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Прослушивание порта
    if (listen(server, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }


     while (1) {
        if ((new_client_socket = accept(server, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Обработка подключений с помощью fork()
        if (fork() == 0) {
            close(server); // Закрыть родительский сокет в дочернем процессе

            // Чтение и вывод сообщений
            int read_bytes;
            while ((read_bytes = read(new_client_socket, buffer, BUFFER_SIZE)) > 0) {
                buffer[read_bytes] = '\0';
                std::cout << "Received message: " << buffer << std::endl;
            }

            close(new_client_socket); // Закрыть дочерний сокет
            exit(EXIT_SUCCESS);
        } else {
            close(new_client_socket); // Закрыть дочерний сокет в родительском процессе
            waitpid(-1, nullptr, WNOHANG); // Очистка завершившихся дочерних процессов
        }
    }
    return 0;
}