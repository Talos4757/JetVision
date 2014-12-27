#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>

#include "Improc.cpp"

#define TCP_SOCKET 0
#define JPORT 4242
#define RIO_IP "10.47.57.2"

using namespace std;

class JetClient
{
public:
  static bool SendTargets(vector<Target*> targets);

private:
  static int RioSocket;

  static bool Init();
  static char* Seriallize(Target *t);
};
