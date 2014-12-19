struct UpdaterStruct
{
  VideoCapture *vidCap;
  Mat *frame;
  //GpuMat *gpuFrame;
  pthread_mutex_t *frameLocker;
};

enum TargetType
{
  //MUST be ordered by in game points value (best target is last)
  LowGoalR = 1,
  LowGoalL,
  HighGoalR,
  HighGoalL,
  Truss,
  HotGoal
};

class Target
{
  //ALL numbers should be relative to the driving pivot!
  public:
    TargetType type;
    double distance;
    double height;
    double h_angle;
    double v_angle;

    string Serialize()
    {
      ostringstream s;
      s << distance << " " << height << " " << h_angle << " "
               << v_angle << " " << type;
      return s.str();
    }
};
