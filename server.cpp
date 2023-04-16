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

    send(new_client_socket, message.c_str(), message.length(), 0);
}

uint16_t calculate_hash(const string &username)
{
    uint16_t sum = 0;
    for (const char &c : username)
    {
        sum += static_cast<uint16_t>(static_cast<unsigned char>(c));
    }
    return ((sum * 1000) % 65536)+23000;
}

bool client_authenticate(int new_client_socket)
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
        send_message(new_client_socket, "303 KEY OUT OF RANGE\a\b");
        close(new_client_socket);
        return false;
    }
    // Retrieve server and client keys for the given Key ID
    uint16_t server_key = keys[client_key_id].first;
    uint16_t client_key = keys[client_key_id].second;

    uint16_t username_hash = calculate_hash(client_username);
    cout << username_hash << endl;
    cout << server_key << endl;
    // Calculate server confirmation code
    uint16_t server_confirmation_code = (username_hash + server_key) % 65536;
    cout << server_confirmation_code << endl;
    // SERVER_CONFIRMATION
    string conf = to_string(server_confirmation_code) + "\a\b";
    send_message(new_client_socket, conf);
    // CLIENT_CONFIRMATION
    string client_confirmation = receive_message(new_client_socket);
    cout << client_confirmation << endl;
    uint16_t client_confirmation_code = static_cast<uint16_t>(stoi(client_confirmation));
    // Calculate expected client confirmation code
    uint16_t expected_client_confirmation_code = (username_hash + client_key) % 65536;
    if (client_confirmation_code == expected_client_confirmation_code)
    {
        // SERVER_OK
        send_message(new_client_socket, "200 OK\a\b");
        return true;
    }
    else
    {
        // SERVER_LOGIN_FAILED
        send_message(new_client_socket, "300 LOGIN FAILED\a\b");
        return false;
    }
}

void handle_client(int new_client_socket)
{
    // Authentication process
    if(!client_authenticate(new_client_socket)){
        close(new_client_socket);
        return;
    }
    cout << "AAAAAAA" << endl;
    send_message(new_client_socket, "102 MOVE\a\b");
    // Main loop for client communication
    int previous_x = 0, previous_y = 0;
    int obstacle_hit_count = 0;
    while (true)
    {
        string message = receive_message(new_client_socket);
        stringstream ss(message);
        string message_type;
        ss >> message_type;

        if (message_type == "OK")
        {
            int x, y;
            ss >> x >> y;

            // Check if the robot reached the target coordinate
            if (x == 0 && y == 0)
            {
                send_message(new_client_socket, "105 GET MESSAGE\a\b");
            }
            else
            {
                // If the robot's position has not changed, it has hit an obstacle
                if (x == previous_x && y == previous_y)
                {
                    obstacle_hit_count++;
                    if (obstacle_hit_count > 20)
                    {
                        close(new_client_socket);
                        return;
                    }
                    send_message(new_client_socket, "104 TURN RIGHT\a\b");
                }
                else
                {

                    // Update previous coordinates
                    previous_x = x;
                    previous_y = y;

                    // Handle the robot's position update
                    // Decide the next movement command (SERVER_MOVE, SERVER_TURN_LEFT, SERVER_TURN_RIGHT)
                    // and send it to the client
                    send_message(new_client_socket, "102 MOVE\a\b");
                }
            }
        }
        else if (message_type == "RECHARGING")
        {
            // Handle the robot recharging
        }
        else if (message_type == "FULL")
        {
            // Check if the message is "FULL POWER\a\b"
            string power;
            ss >> power;
            if (power == "POWER")
            {
                // Handle the robot returning to full power
            }
        }
        else
        {
            string client_message;
            getline(ss, client_message, '\a');
            // Handle the received secret message

            // Send SERVER_LOGOUT to end communication
            send_message(new_client_socket, "106 LOGOUT\a\b");
            close(new_client_socket);
            return;
        }
    }
}

int main(int argc, char **argv)
{
    sockaddr_in address;
    int server, new_client_socket, port;
    cout << "1" << endl;
    if (argc < 2)
    {
        perror("Usage: server port");
        exit(EXIT_FAILURE);
    }
    cout << "2" << endl;
    if ((server = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
    cout << "3" << endl;
    port = atoi(argv[1]);
    if (port == 0)
    {
        close(server);
        perror("Usage: server port");
        exit(EXIT_FAILURE);
    }
    cout << "4" << endl;
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