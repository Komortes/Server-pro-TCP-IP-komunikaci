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
#include <regex>
#include <chrono>
#include <queue>
using namespace std;

constexpr int TIMEOUT_MS = 1000;
constexpr int TIMEOUT_RECHARGING_MS = 5000;
constexpr int BUFFER_SIZE = 1024;

enum class Direction
{
    UP,
    DOWN,
    LEFT,
    RIGHT,
    NONE
};

enum class Phase
{
    Auth,
    Codes,
    Moving,
    End
};

enum class State
{
    Run,
    Wait,
    Resume
};

struct ClientData
{
    int socket;
    queue<vector<char>> messages;
    vector<char> partial_message;
    Phase cur;
    State us_st;
    bool last_awaiting;
};

struct RobotState
{
    int x;
    int y;
    Direction direction;
    bool obstacle_mode;
};

map<uint16_t, pair<uint16_t, uint16_t>> keys = {
    {0, {23019, 32037}},
    {1, {32037, 29295}},
    {2, {18789, 13603}},
    {3, {16443, 29533}},
    {4, {18189, 21952}},
};

void send_message(int client_socket, const string &message)
{
    send(client_socket, message.c_str(), message.length(), 0);
}

vector<char> receive_message(ClientData &client)
{

    if (!client.messages.empty())
    {
        auto message = client.messages.front();
        client.messages.pop();
        if (client.cur == Phase::Auth && message.size() >= 20)
        {
            send_message(client.socket, "301 SYNTAX ERROR\a\b");
            close(client.socket);
            return vector<char>();
        }
        else if (client.cur == Phase::Moving && message.size() >= 12)
        {
            send_message(client.socket, "301 SYNTAX ERROR\a\b");
            close(client.socket);
            return vector<char>();
        }
        else if (client.cur == Phase::End && message.size() >= 100)
        {
            send_message(client.socket, "301 SYNTAX ERROR\a\b");
            close(client.socket);
            return vector<char>();
        }
        return message;
    }
    char buffer[BUFFER_SIZE] = {0};

    struct pollfd pfd;
    pfd.fd = client.socket;
    pfd.events = POLLIN;
    int lenght_c = 0;
    vector<char> partial_message = client.partial_message;
    bool awaiting_ab = client.last_awaiting;
    auto last_communication_time = chrono::steady_clock::now();
    auto waiting_start = chrono::steady_clock::now(); 


    while (true)
    {
        int poll_result = poll(&pfd, 1, 0);

        if (poll_result > 0 && (pfd.revents & POLLIN))
        {
            int read_bytes = read(client.socket, buffer, BUFFER_SIZE);
            last_communication_time = chrono::steady_clock::now();

            for (int i = 0; i < read_bytes; i++)
            {
                if (awaiting_ab && buffer[i] == '\b')
                {
                    lenght_c++;
                    partial_message.pop_back(); 

                    string message(partial_message.begin(), partial_message.end());
                    vector<regex> patterns;

                    if (client.cur == Phase::Auth)
                    {
                        patterns = {
                            regex(R"(^(?!.*\\a\\b)(.{1,18})$)"),
                            regex(R"(^RECHARGING$)"),
                            regex(R"(^FULL POWER$)")};
                    }
                    else if (client.cur == Phase::Codes)
                    {
                        patterns = {
                            regex(R"(^(\d{1,3})$)"),
                            regex(R"(^(\d{1,5})$)"),
                            regex(R"(^RECHARGING$)"),
                            regex(R"(^FULL POWER$)")};
                    }
                    else if (client.cur == Phase::Moving)
                    {
                        patterns = {
                            regex(R"(^OK (-?\d+) (-?\d+)$)"),
                            regex(R"(^RECHARGING$)"),
                            regex(R"(^FULL POWER$)")};
                    }
                    else
                    {
                        patterns = {
                            regex(R"(^(?!.*\\a\\b)(.{1,98})$)"),
                            regex(R"(^RECHARGING$)"),
                            regex(R"(^FULL POWER$)")};
                    }

                    bool match_found = false;
                    for (const auto &pattern : patterns)
                    {
                        if (regex_match(message, pattern))
                        {
                            match_found = true;
                            break;
                        }
                    }

                    if (match_found)
                    {
                        if (message == "RECHARGING")
                        {
                            client.us_st = State::Wait;
                            lenght_c = 0;
                        }
                        else if (message == "FULL POWER")
                        {
                            client.us_st = State::Resume;
                            last_communication_time = chrono::steady_clock::now();
                            lenght_c = 0;
                        }
                        else if (message != "FULL POWER" && client.us_st == State::Wait)
                        {
                            send_message(client.socket, "302 LOGIC ERROR\a\b");
                            close(client.socket);
                            return vector<char>();
                        }

                        partial_message.clear();
                        if (client.us_st == State::Run || (client.us_st == State::Resume && message != "FULL POWER"))
                        {
                            client.messages.push(vector<char>(message.begin(), message.end()));
                        }

                        if (i < read_bytes - 1)
                        {
                            lenght_c = 0;
                            i++;
                            client.partial_message.clear();
                            partial_message.push_back(buffer[i]);
                        }
                    }
                    else
                    {

                        send_message(client.socket, "301 SYNTAX ERROR\a\b");
                        close(client.socket);
                        return vector<char>();
                    }

                    awaiting_ab = false;
                }
                else
                {
                    lenght_c++;
                    if (((client.cur == Phase::Auth && lenght_c >= 20) || (client.cur == Phase::Moving && lenght_c >= 12) || (client.cur == Phase::End && lenght_c >= 100)) && client.us_st == State::Run)
                    {
                        send_message(client.socket, "301 SYNTAX ERROR\a\b");
                        close(client.socket);
                        return vector<char>();
                    }
                    if (buffer[i] == '\a')
                    {
                        awaiting_ab = true;
                    }
                    else
                    {
                        awaiting_ab = false;
                    }

                    partial_message.push_back(buffer[i]);
                }
                client.last_awaiting = awaiting_ab;
            }
        }
        else
        {
            auto now = chrono::steady_clock::now();
            auto elapsed_time = chrono::duration_cast<chrono::milliseconds>(now - last_communication_time);

            if (elapsed_time.count() >= TIMEOUT_MS && client.us_st == State::Run)
            {
                close(client.socket);
                return vector<char>();
            }

            if (client.us_st == State::Wait)
            {
                auto elapsed_waiting_time = chrono::duration_cast<chrono::milliseconds>(now - waiting_start);
                if (elapsed_waiting_time.count() >= TIMEOUT_RECHARGING_MS)
                {
                    close(client.socket);
                    return vector<char>();
                }
            }
        }

        if (!client.messages.empty() && client.us_st != State::Wait)
        {

            if (!partial_message.empty())
            {
                client.partial_message = partial_message;
            }
            else
            {
                client.partial_message.clear();
            }
            auto message = client.messages.front();
            client.messages.pop();
            string message_str(message.begin(), message.end());
            if (client.us_st == State::Resume)
            {
                client.us_st = State::Run;
                client.partial_message.clear();
            }
            return message;
        }
    }
}

uint16_t calculate_hash(const vector<char> &username)
{
    uint16_t sum = 0;
    for (const char &c : username)
    {
        sum += static_cast<uint16_t>(static_cast<unsigned char>(c));
    }
    return ((sum * 1000) % 65536);
}

bool client_authenticate(ClientData &client_data)
{
    client_data.cur = Phase::Auth;
    vector<char> client_username = receive_message(client_data);
    string client_username_str(client_username.begin(), client_username.end());
    if (client_username.empty())
    {
        send_message(client_data.socket, "300 LOGIN FAILED\a\b");
        close(client_data.socket);
        return false;
    }

    send_message(client_data.socket, "107 KEY REQUEST\a\b");
    client_data.cur = Phase::Codes;

    vector<char> client_key_id_vec = receive_message(client_data);
    if (client_key_id_vec.empty())
    {
        send_message(client_data.socket, "300 LOGIN FAILED\a\b");
        close(client_data.socket);
        return false;
    }
    string client_key_id_str(client_key_id_vec.begin(), client_key_id_vec.end());
    uint16_t client_key_id = stoul(client_key_id_str);

    if (keys.find(client_key_id) == keys.end())
    {
        send_message(client_data.socket, "303 KEY OUT OF RANGE\a\b");
        close(client_data.socket);
        return false;
    }
    uint16_t server_key = keys[client_key_id].first;
    uint16_t client_key = keys[client_key_id].second;

    uint16_t username_hash = calculate_hash(client_username);
    uint16_t server_confirmation_code = (username_hash + server_key) % 65536;
    string conf = to_string(server_confirmation_code) + "\a\b";
    send_message(client_data.socket, conf);
    vector<char> client_confirmation = receive_message(client_data);
    if (client_confirmation.empty())
    {
        return false;
    }

    string client_confirmation_str(client_confirmation.begin(), client_confirmation.end());
    uint16_t client_confirmation_code = 0;

    stringstream ss(client_confirmation_str);
    ss >> client_confirmation_code;
    uint16_t expected_client_confirmation_code = (username_hash + client_key) % 65536;
    if (client_confirmation_code == expected_client_confirmation_code)
    {
        send_message(client_data.socket, "200 OK\a\b");
        return true;
    }
    else
    {
        send_message(client_data.socket, "300 LOGIN FAILED\a\b");
        return false;
    }
}

void handle_client(ClientData client_data)
{
    if (!client_authenticate(client_data))
    {
        close(client_data.socket);
        return;
    }
    client_data.cur = Phase::Moving;
    send_message(client_data.socket, "102 MOVE\a\b");
    int previous_x = 0, previous_y = 0;
    int obstacle_hit_count = 0;
    bool turn = false;
    chrono::steady_clock::time_point recharging_start;
    Direction cur_dir = Direction::NONE;

    while (true)
    {
        vector<char> message_vec = receive_message(client_data);
        string message(message_vec.begin(), message_vec.end());
        stringstream ss(message);
        string message_type;
        ss >> message_type;

        if (message_type == "OK")
        {

            int x, y;
            ss >> x >> y;
            if (x == 0 && y == 0)
            {
                client_data.cur = Phase::End;
                send_message(client_data.socket, "105 GET MESSAGE\a\b");
            }
            else
            {
                if (x == previous_x && y == previous_y && !turn)
                {
                    obstacle_hit_count++;
                    if (obstacle_hit_count > 20)
                    {
                        close(client_data.socket);
                        return;
                    }

                    send_message(client_data.socket, "104 TURN RIGHT\a\b");
                    receive_message(client_data);
                    send_message(client_data.socket, "102 MOVE\a\b");
                    receive_message(client_data);
                    send_message(client_data.socket, "103 TURN LEFT\a\b");
                    receive_message(client_data);
                    send_message(client_data.socket, "102 MOVE\a\b");
                    receive_message(client_data);
                    send_message(client_data.socket, "102 MOVE\a\b");
                    receive_message(client_data);
                    if (y == 0 || x == 0)
                    {
                        send_message(client_data.socket, "103 TURN LEFT\a\b");
                        receive_message(client_data);
                        send_message(client_data.socket, "102 MOVE\a\b");
                        receive_message(client_data);
                        send_message(client_data.socket, "104 TURN RIGHT\a\b");
                    }
                    else
                    {
                        send_message(client_data.socket, "102 MOVE\a\b");
                    }
                }
                else
                {

                    if (cur_dir != Direction::NONE)
                    {
                        if (x < previous_x && previous_y == y)
                        {
                            cur_dir = Direction::LEFT;
                        }
                        else if (x > previous_x && previous_y == y)
                        {
                            cur_dir = Direction::RIGHT;
                        }
                        else if (y < previous_y && previous_x == x)
                        {
                            cur_dir = Direction::DOWN;
                        }
                        else if (y > previous_y && previous_x == x)
                        {
                            cur_dir = Direction::UP;
                        }
                    }

                    previous_x = x;
                    previous_y = y;
                    if (x > 0)
                    {
                        if (cur_dir == Direction::RIGHT)
                        {
                            send_message(client_data.socket, "104 TURN RIGHT\a\b");
                            receive_message(client_data);
                            send_message(client_data.socket, "104 TURN RIGHT\a\b");
                            turn = true;
                        }
                        else if (cur_dir == Direction::UP)
                        {
                            send_message(client_data.socket, "103 TURN LEFT\a\b");
                            turn = true;
                        }
                        else if (cur_dir == Direction::DOWN)
                        {
                            send_message(client_data.socket, "104 TURN RIGHT\a\b");
                            turn = true;
                        }
                        else
                        {
                            send_message(client_data.socket, "102 MOVE\a\b");
                            turn = false;
                        }
                        cur_dir = Direction::LEFT;
                        continue;
                    }
                    else if (x < 0)
                    {
                        if (cur_dir == Direction::LEFT)
                        {
                            send_message(client_data.socket, "104 TURN RIGHT\a\b");
                            receive_message(client_data);
                            send_message(client_data.socket, "104 TURN RIGHT\a\b");
                            turn = true;
                        }
                        else if (cur_dir == Direction::UP)
                        {
                            send_message(client_data.socket, "104 TURN RIGHT\a\b");
                            turn = true;
                        }
                        else if (cur_dir == Direction::DOWN)
                        {
                            send_message(client_data.socket, "103 TURN LEFT\a\b");
                            turn = true;
                        }
                        else
                        {
                            send_message(client_data.socket, "102 MOVE\a\b");
                            turn = false;
                        }
                        cur_dir = Direction::RIGHT;
                        continue;
                    }

                    if (x == 0)
                    {
                        if (y > 0)
                        {
                            if (cur_dir == Direction::RIGHT)
                            {
                                send_message(client_data.socket, "104 TURN RIGHT\a\b");
                                turn = true;
                            }
                            else if (cur_dir == Direction::UP)
                            {
                                send_message(client_data.socket, "104 TURN RIGHT\a\b");
                                receive_message(client_data);
                                send_message(client_data.socket, "104 TURN RIGHT\a\b");
                                turn = true;
                            }
                            else if (cur_dir == Direction::LEFT)
                            {
                                send_message(client_data.socket, "103 TURN LEFT\a\b");
                                turn = true;
                            }
                            else
                            {
                                send_message(client_data.socket, "102 MOVE\a\b");
                                turn = false;
                            }
                            cur_dir = Direction::DOWN;
                            continue;
                        }
                        else if (y < 0)
                        {
                            if (cur_dir == Direction::RIGHT)
                            {
                                send_message(client_data.socket, "103 TURN LEFT\a\b");
                                turn = true;
                            }
                            else if (cur_dir == Direction::DOWN)
                            {
                                send_message(client_data.socket, "104 TURN RIGHT\a\b");
                                receive_message(client_data);
                                send_message(client_data.socket, "104 TURN RIGHT\a\b");
                                turn = true;
                            }
                            else if (cur_dir == Direction::LEFT)
                            {
                                send_message(client_data.socket, "104 TURN RIGHT\a\b");
                                turn = true;
                            }
                            else
                            {
                                send_message(client_data.socket, "102 MOVE\a\b");
                                turn = false;
                            }
                            cur_dir = Direction::UP;
                            continue;
                        }
                    }
                }
            }
        }
        else
        {
            string client_message;
            getline(ss, client_message, '\a');
            send_message(client_data.socket, "106 LOGOUT\a\b");
            close(client_data.socket);
            return;
        }
    }
}

int main(int argc, char **argv)
{
    sockaddr_in address;
    int server, new_client_socket, port;
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

        ClientData client_data;
        client_data.socket = new_client_socket;
        client_data.last_awaiting = false;
        client_data.us_st = State::Run;
        thread client_thread(handle_client, client_data);
        client_thread.detach();
    }
    return 0;
}