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
  LowGoalR,
  LowGoalL,
  HighGoalR,
  HighGoalL,
  Truss,
  HotGoal
};

struct Target
{
  //ALL numbers should be relative to the driving pivot!
  TargetType type;
  double distance;
  double height;
  double h_angle;
  double v_angle;
};
