#include <cstdlib>
#include <cstdio>
#include <sys/socket.h> // socket(), bind(), connect(), listen()
#include <unistd.h> // close(), read(), write()
#include <netinet/in.h> // struct sockaddr_in
#include <strings.h> // bzero()
#include <wait.h> // waitpid()
#include <iostream>
using namespace std;

int main(int argc, char **argv){

    if(argc < 2){
        return -1;
    }

    while(true){


    }
    return 0;
}