# C++ Server Application

## Description

This is a server-side application written in C++. The application uses socket programming to establish and manage TCP connections with clients. It is designed to handle multiple clients concurrently using threads.

The server application follows a specific protocol to interact with clients:

1. **Authentication**: The server receives a username from the client, then it sends a key request to the client. The client responds with a key ID, and the server calculates a confirmation code based on the username and the server key associated with the key ID. The server sends the confirmation code to the client and expects a confirmation code from the client. If the client's confirmation code is correct, the server sends an "OK" message to the client.

2. **Moving**: The server sends a "MOVE" command to the client and expects an "OK" message with the client's new coordinates. If the client hits an obstacle, the server sends a series of commands to the client to navigate around the obstacle.

3. **End**: When the client reaches the destination (0, 0), the server sends a "GET MESSAGE" command to the client and expects a message from the client. After receiving the client's message, the server sends a "LOGOUT" command to the client and closes the connection.

The server application handles syntax errors, logic errors, and key out of range errors by sending appropriate error messages to the client and closing the connection.

## Building and Running the Server

To build and run the server application:

1. Clone the repository to your local machine.
2. Navigate to the directory containing the source code.
3. Compile the source code using a C++ compiler. For example, if you are using the g++ compiler, you can use the following command:

    ```
    g++ -o server server.cpp -lpthread
    ```

    This command compiles the source code into an executable file named "server" and links the pthread library for multithreading.

4. Run the server application by executing the following command:

    ```
    ./server <port>
    ```

    Replace `<port>` with the port number that the server should listen on.

## Usage

After starting the server application, it listens for incoming connections on the specified port. When a client connects to the server, the server creates a new thread to handle the client. The server then follows the protocol described above to interact with the client.
