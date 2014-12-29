#include "JetClient.h"

bool JetClient::SendTargets(vector<Target*> targets)
{
    int target_count = targets.size();
    send(RioSocket,(char*)target_count,sizeof(int),0);

    for(int i = 0; i < target_count; i++)
    {
        char* encodedTarget = Serialize(targets[i]);
        send(RioSocket,encodedTarget,TARGETSIZE,0);
        delete[] encodedTarget;
    }
}

bool JetClient::Init()
{
    if(RioSocket = socket(AF_INET,SOCK_STREAM,TCP_SOCKET) < 0)
    {
        return false;
    }

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(JPORT);
    sa.sin_addr.s_addr = inet_addr(RIO_IP);

    if(connect(RioSocket,(struct sockaddr*)&sa, sizeof(sa)) != 0)
    {
        return false;
    }

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
