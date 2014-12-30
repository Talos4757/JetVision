#ifndef JETCLIENT_H
#define JETCLIENT_H

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>

#include "Utility.h"
#include <netinet/in.h>
#include <arpa/inet.h>

#define TCP_SOCKET 0
#define JPORT 4242
#define RIO_IP "10.47.57.2"

using namespace std;

class JetClient
{
public:
	static bool Init();
    static bool SendTargets(vector<Target*> targets);

private:
    static int RioSocket;    
    static char* Serialize(Target *t);
};

#endif
