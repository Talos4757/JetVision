#include <sys/socket.h>
#include <sys/types.h>

#include "Improc.cpp"


int main(int argc, char* argv[])
{
  //TODO Bind and stuff
  StartVision(argc, argv);
}

void SendData(vector<Target> targets)
{
  for(int i = 0; i < targets.size(); i++)
  {
    string s = targets[i].Serialize();
    //SEND s
  }
}


