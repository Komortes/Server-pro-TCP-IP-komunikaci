#include <cstdlib>
#include <arpa/inet.h>
#include <cstdio>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <strings.h>
#include <wait.h>
#include <iostream>
#include <thread>
#include <poll.h>
#include <string>
#include <map>
#include <vector>
#include <sstream>
using namespace std;

constexpr int TIMEOUT = 10 * 1000;
constexpr int BUFFER_SIZE = 1024;

map<uint16_t, pair<uint16_t, uint16_t>> keys = {
    {0, {23019, 32037}},
    {1, {32037, 29295}},
    {2, {18789, 13603}},
    {3, {16443, 29533}},
    {4, {18189, 21952}},
};

string receive_message(int new_client_socket)
{
    char buffer[BUFFER_SIZE] = {0};
    int read_bytes = read(new_client_socket, buffer, BUFFER_SIZE);
    buffer[read_bytes] = '\0';

    string received_message(buffer);

    // Remove the termination sequence '\a\b' from the received message
    size_t termination_pos = received_message.find("\a\b");
    if (termination_pos != string::npos)
    {
        received_message.erase(termination_pos);
    }

    return received_message;
}

void send_message(int new_client_socket, const string &message)
{
    // Add the termination sequence '\a\b' to the message
    string message_with_termination = message + "\a\b";
    send(new_client_socket, message_with_termination.c_str(), message_with_termination.length(), 0);
}

uint16_t calculate_hash(const string &username)
{
    uint16_t sum = 0;
    for (const char &c : username)
    {
        sum += static_cast<uint16_t>(c);
    }
    return (sum * 1000) % 65536;
}

void client_authenticate(int new_client_socket)
{
    // CLIENT_USERNAME
    string client_username = receive_message(new_client_socket);

    // SERVER_KEY_REQUEST
    send_message(new_client_socket, "107 KEY REQUEST\a\b");

    // CLIENT_KEY_ID
    string client_key_id_str = receive_message(new_client_socket);
    uint16_t client_key_id = stoul(client_key_id_str);

    // Check if the provided Key ID exists in the keys map
    if (keys.find(client_key_id) == keys.end())
    {
        // Invalid Key ID, send SERVER_LOGIN_FAILED and close connection
        send_message(new_client_socket, "300 LOGIN FAILED\a\b");
        close(new_client_socket);
        return;
    }

    // Retrieve server and client keys for the given Key ID
    uint16_t server_key = keys[client_key_id].first;
    uint16_t client_key = keys[client_key_id].second;

    uint16_t username_hash = calculate_hash(client_username);

    // Calculate server confirmation code
    uint16_t server_confirmation_code = (username_hash + server_key) % 65536;

    // SERVER_CONFIRMATION
    send_message(new_client_socket, to_string(server_confirmation_code) + "\a\b");

    // CLIENT_CONFIRMATION
    string client_confirmation = receive_message(new_client_socket);
    uint16_t client_confirmation_code = stoul(client_confirmation);

    // Calculate expected client confirmation code
    uint16_t expected_client_confirmation_code = (username_hash + client_key) % 65536;

    if (client_confirmation_code == expected_client_confirmation_code)
    {
        // SERVER_OK
        send_message(new_client_socket, "200 OK\a\b");
    }
    else
    {
        // SERVER_LOGIN_FAILED
        send_message(new_client_socket, "300 LOGIN FAILED\a\b");
    }
}

void handle_client(int new_client_socket)
{
    // Authentication process
    client_authenticate(new_client_socket);

    send_message(new_client_socket, "SERVER_MOVE");
    // Main loop for client communication
    while (true)
    {
        string message = receive_message(new_client_socket);
        stringstream ss(message);
        string message_type;
        ss >> message_type;

        if (message_type == "CLIENT_OK")
        {
            int x, y;
            ss >> x >> y;

            // Handle the robot's position update
            // Decide the next movement command (SERVER_MOVE, SERVER_TURN_LEFT, SERVER_TURN_RIGHT)
            // and send it to the client

            send_message(new_client_socket, "SERVER_MOVE");
        }
        else if (message_type == "CLIENT_RECHARGING")
        {
            // Handle the robot recharging
        }
        else if (message_type == "CLIENT_FULL_POWER")
        {
            // Handle the robot returning to full power
        }
        else if (message_type == "CLIENT_MESSAGE")
        {
            string client_message;
            getline(ss, client_message);
            // Handle the received secret message
        }
        else
        {
            // Handle unknown or unexpected messages
        }
    }
}

int main(int argc, char **argv)
{
    sockaddr_in address;
    int server, new_client_socket, port;
    char buffer[BUFFER_SIZE] = {0};
    if (argc < 2)
    {
        perror("Usage: server port");
        exit(EXIT_FAILURE);
    }

    if ((server = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[1]);
    if (port == 0)
    {
        close(server);
        perror("Usage: server port");
        exit(EXIT_FAILURE);
    }

    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    int addrlen = sizeof(address);

    if (bind(server, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        if ((new_client_socket = accept(server, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("Accept error");
            exit(EXIT_FAILURE);
        }

        thread client_thread(handle_client, new_client_socket);
        client_thread.detach();
    }
    return 0;
}