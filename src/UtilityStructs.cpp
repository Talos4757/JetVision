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
	NA = 0,
	LowGoalR,
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
    TargetType type = NA;
    double distance;
    double h_angle;
    double v_angle;

    string Serialize() //TODO actual bytes serialization
    {
      ostringstream s;
      s << distance << " " << h_angle << " " << v_angle << " " << type;
      return s.str();
    }
};
