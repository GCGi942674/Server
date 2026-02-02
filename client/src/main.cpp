#include "echo_client.h"
#include <iostream>
#include <string>

using namespace std;

int main(){
    EchoClient client("127.0.0.1", 8080);
    if(!client.connect()){
        cerr << "Failed to connect to server." << endl;
        return 1;
    }

    string input;
    while(true){
        cout << "Client: ";
        getline(cin, input);
        if(input == "exit") break;

        string response;
        if(!client.sendMessage(input, response)){
            cerr << "Communication error." << endl;
            break;
        }
        cout << "Server: " << response << endl;
    }

    client.disconnect();
    return 0;
}