/*
    Matthew Schutz
    CS4850
    ProjectV2
    03/13/2023
    This project will run a client in a chat room application using sockets. The client can create a new user, login, see who is in the chat room,
    send a message to all users, send a message to a specific user, and logout.
 */

#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <sstream>
#include <thread>

#define PORT 16271
#define MAX_LINE 294 // For send userID must be able to support the length of the command(4) + length of userID(up to 32) + length of message(up to 256) + spaces between(2)

void receiving(int s);
void sending(int s);
int login(char* buf, int s);
int newuser(char* buf, int s);
int sendMessage(char* buf, int s);
int logout(char* buf, int s);
int checkLogins(char* buf, char* commandUsage);
int who(char* buf, int s);

bool loggedIn = false;

int main(int argc, const char * argv[]) {
    // checks for correct amount of arguments.
    if(argc < 2){
        std::cerr << "Usage: client serverName" << std::endl;
        return -1;
    }
    
    // creates the ip address from input
    unsigned int ipaddr;
    if (isalpha(argv[1][0])) {
        hostent* remoteHost = gethostbyname(argv[1]);
        if (remoteHost == NULL) {
            std::cerr << "Host not found" << std::endl;
            return -1;
        }
        ipaddr = *((unsigned long *) remoteHost->h_addr);
    } else {
        ipaddr = inet_addr(argv[1]);
    }
    
    // create a socket using IPv4 and TCP. Checks to make sure it was successfully created
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if(s == -1){
        std::cerr << "Error at socket()" << std::endl;
        return -1;
    }
    
    // connect to a server using the ipaddr given in the command line. Checks to ensure it was successful
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ipaddr;
    addr.sin_port = htons(PORT);
    if (connect(s, (sockaddr*)&addr, sizeof(addr)) == -1) {
        std::cerr << "Failed to connect." << std::endl;
        return -1;
    }
    
    std::cout << "My chat room client. Version Two.  \n" << std::endl;
    
    // file descriptor to wait for input from either socket or Standard In
    fd_set read_fds;
    
    while(true){
        // clear the fd
        FD_ZERO(&read_fds);
        // for standard in
        FD_SET(STDIN_FILENO, &read_fds);
        // for socket
        FD_SET(s, &read_fds);
        // wait for input from either source and check for errors
        int result = select(std::max(STDIN_FILENO, s)+1, &read_fds, nullptr, nullptr, nullptr);
        if(result == -1){
            std::cerr << "Error calling select" << std::endl;
            close(s);
            return -1;
        }
        
        // if the input came from standard in
        if(FD_ISSET(STDIN_FILENO, &read_fds)){
            // take input
            char buf[MAX_LINE];
            fgets(buf, sizeof(buf), stdin);
            // separate the command
            std::stringstream inputStream(buf);
            std::string command;
            inputStream >> command;
    
            // calls function based on given command
            if (command == "login") {
                login(buf, s);
            } else if(command == "newuser"){
                newuser(buf, s);
            } else if(command == "send"){
                sendMessage(buf, s);
            } else if(command == "logout"){
                if(logout(buf, s) == 0){
                    // if logout, close the client
                    break;
                }
            } else if(command == "who"){
                who(buf, s);
            } else {
                std::cout << "Invalid command. List of commands are \"login\", \"newuser\", \"send\", and \"logout\"" << std::endl;
            }
        }
        // if input is from socket
        else if(FD_ISSET(s, &read_fds)){
            // receive on the socket and check for errors
            char buf[MAX_LINE];
            int len = recv(s, buf, MAX_LINE, 0);
            if (len <= 0) {
                std::cerr << "error receiving message" << std::endl;
                close(s);
                return -1;
            }
            // output the message from the socket
            buf[len] = '\0';
            std::cout << buf << std::endl;
            char maxClientMsg[] = "Maximum number of clients already connected to server. Please try again later.\0";
            if(strncmp(buf, maxClientMsg, strlen(maxClientMsg)) == 0){
                break;
            }
        }
    }
    close(s);
}

// sends a message to the server. Checks to ensure for proper usage and login status. Will return 0 on success and -1 on failure.
int sendMessage(char* buf, int s){
    if(!loggedIn){
        std::cout << "Denied. Please login first." << std::endl;
        return -1;
    }
    // separate the input out to check for proper usage
    std::stringstream inputStream(buf);
    std::string command, recipient, msg;
    inputStream >> command;
    inputStream >> recipient;
    inputStream >> msg;
    // check message length
    if (msg.length() < 1) {
        std::cout << "Message must be between length 1 and 256 characters." << std::endl;
        return -1;
    }
    // check recipient length
    if(recipient.length() > 32 || recipient.length() < 3){
        std::cout << "UserID's are between 3 and 32 characters long" << std::endl;
        return -1;
    }
    // send the message
    send(s, buf, strlen(buf), 0);
    
    return 0;
}

// logs a user out. checks to ensure there is a logged in user first, and then also checks to ensure the logout was successful on the server side. Prints out
// confirmation message. Returns 0 on success and -1 on failure
int logout(char* buf, int s){
    if(!loggedIn){
        std::cout << "Denied. Please login first." << std::endl;
        return -1;
    }
    // checks for proper usage
    std::stringstream inputStream(buf);
    std::string command, otherInp;
    inputStream >> command;
    inputStream >> otherInp;
    if(otherInp.length() != 0){
        std::cout << "Correct usage: logout" << std::endl;
        return -1;
    }
    
    // send the logout message to server
    send(s, buf, strlen(buf), 0);
    int len = recv(s, buf, MAX_LINE, 0);
    buf[len] = 0;
    // check to see if the server confirmed this logout
    std::cout << buf << std::endl;
    if (strncmp(buf, "Logout failed.", 14) == 0) {
        return -1;
    }
    // update logged in status
    loggedIn = false;
    return 0;
}

// creates a new user. Checks to ensure the command is used properly, and that a user is not logged in. Then sends the request to the server and prints out
// the response from the server. Returns 0 on success and -1 on failure.
int newuser(char* buf, int s){
    char newuserErrMess[64];
    strcpy(newuserErrMess, "Correct usage: newuser userID password");
    if (checkLogins(buf, newuserErrMess) == -1) {
        return -1;
    }
    
    send(s, buf, strlen(buf), 0);
    int len = recv(s, buf, MAX_LINE, 0);
    buf[len] = 0;
    std::cout << buf << std::endl;
    if (buf[0] != 'N') {
        return -1;
    }
    
    return 0;
}

// logs a user in. First checks for proper usage and login status and then sends the request to the server. The function will update logged in status based off of the
// received message back from the server and print it out. Returns 0 on success and -1 on failure.
int login(char* buf, int s){
    char loginErrMess[64];
    strcpy(loginErrMess, "Correct usage: login userID password");
    // check for correct logins
    if (checkLogins(buf, loginErrMess) == -1) {
        return -1;
    }
    // send the login message
    send(s, buf, strlen(buf), 0);
    // receive confirmation or denial from server
    int len = recv(s, buf, MAX_LINE, 0);
    buf[len] = 0;
    // outputs the servers response and updates login status if necessary
    std::cout << buf << std::endl;
    if (buf[0] != 'l') {
        return -1;
    }
    loggedIn = true;
    return 0;
}

// queries the server for a list of currently logged in users. Outputs that list to the client.
// returns a 0 on success and -1 on failure.
int who(char* buf, int s){
    if(!loggedIn){
        std::cout << "Denied. Please login first." << std::endl;
        return -1;
    }
    // checks for correct usage
    std::stringstream inputStream(buf);
    std::string command, excess;
    inputStream >> command;
    inputStream >> excess;
    if(excess.length() != 0){
        std::cout << "Correct usage: who" << std::endl;
        return -1;
    }
    // send the command, receive the list, and print it out
    send(s, buf, strlen(buf), 0);
    int len = recv(s, buf, MAX_LINE, 0);
    buf[len] = 0;
    std::cout << buf << std::endl;
    
    return 0;
}

// checks to make sure there is no user logged in and the username and password follow the required restraints.
int checkLogins(char* buf, char* commandUsage){
    if (loggedIn) {
        std::cout << "Denied. User already logged in." << std::endl;
        return -1;
    }
    std::stringstream inputStream(buf);
    std::string command, userID, password, end;
    inputStream >> command;
    inputStream >> userID;
    inputStream >> password;
    inputStream >> end;
    if(password.length() == 0 || end.length() != 0) {
        std::cout << commandUsage << std::endl;
        return -1;
    }
    if (userID.length() < 3 || userID.length() > 32) {
        std::cout << "userID should be between 3 and 32 characters long." << std::endl;
        return -1;
    }
    if (password.length() < 3 || password.length() > 32) {
        std::cout << "password should be between 4 and 8 characters long." << std::endl;
        return -1;
    }
    if(userID == "all"){
        std::cout << "userID cannot be \"all\".";
        return -1;
    }
    return 0;
}

