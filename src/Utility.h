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

    string Serialize();
};

Order4Clockwise(vector<Point2f> corners,Point2f *A, Point2f *C, Point2f *B, Point2f *D) //Left-right + top-down = clockwise
{
	Zero = Point2f(0,0); //Also used later as proxy
	Max = Point2f(H_RES,V_RES);
	A = &Zero, C = &Zero, B = &Max, D = &Max;
	
	for(int i = 0; i < 3; i++)
	{
		if(corners[i].x > C.x)
		{
			C = corners[i];
		}
		else
		{		
			if(corners[i].x > A.x)
			{
				A = corners[i];
			}
		}
		
		if(corners[i].x < B.x)
		{
			B = corners[i];
		}
		else
		{				
			if(corners[i].x < D.x)
			{
				D = corners[i];
			}
		}		
	}
	
	if(A.y < C.y)
	{
		Max = C;
		C = A;
		A = Max;
	}
	
	
	if(B.y < D.y)
	{
		Max = D;
		D = B;
		B = Max;
	}
}
