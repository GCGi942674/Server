#include "EchoClient.h"
#include "logging.h"
#include <atomic>
#include <iostream>
#include <string>
#include <thread>

using namespace std;

int main() {
  EchoClient client("127.0.0.1", 8080);
  if (!client.connect()) {
    cerr << "Failed to connect to server." << endl;
    return 1;
  }
  client.startHeartbeat();

  string input;
  while (true) {
    cout << "Client: ";
    getline(cin, input);
    if (input == "exit")
      break;

    string response;
    if (!client.sendMessage(input, response)) {
      cerr << "Communication error." << endl;
      break;
    }
    cout << "Server: " << response << endl;
  }

  client.disconnect();
  return 0;
}