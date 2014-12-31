#include "JetClient.h"
#include <errno.h>
#include <iostream>

using namespace std;

extern int errno;

int JetClient::RioSocket = 0;

bool JetClient::SendTargets(vector<Target*> targets)
{
    int target_count = targets.size();
    if(send(RioSocket,(char*)&target_count,sizeof(int),0) == -1)
    {
      cout << errno << endl;
	    return false;
    }

    for(int i = 0; i < target_count; i++)
    {
        char* encodedTarget = Serialize(targets[i]);

        if(send(RioSocket,encodedTarget,TARGETSIZE,0)==-1)
        {
		      return false;
        }

        delete[] encodedTarget;
    }

    return true;
}

bool JetClient::Init()
{
    RioSocket = socket(AF_INET,SOCK_STREAM,TCP_SOCKET);

    if(RioSocket < 0)
    {
        return false;
    }

//    cout << errno << endl;
//    cout << RioSocket << endl;

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(JPORT);
    sa.sin_addr.s_addr = inet_addr(RIO_IP);

    while(connect(RioSocket,(struct sockaddr*)&sa, sizeof(sa)) != 0)
    {
	    cout << errno << endl;
    }

    cout << "Connected!" << endl;
    return true;
}

char* JetClient::Serialize(Target *t) //if in the future this function will serialize other types, maybe it should return the length of the returned buffer
{
    char* encoded = new char[TARGETSIZE];

    *(int*)encoded = t->type;
    *(double*)(encoded+sizeof(int)) = t->distance;
    *(double*)(encoded+sizeof(int) + sizeof(double)) = t->h_angle;
    *(double*)(encoded+sizeof(int) + 2*sizeof(double)) = t->v_angle;

    return encoded;
}
