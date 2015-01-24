#ifndef UTILITY_H
#define UTILITY_H

#include <opencv2/opencv.hpp>
using namespace std;
using namespace cv;

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

#define TARGETSIZE (sizeof(int) + 3*sizeof(double))
class Target
{
    //ALL numbers should be relative to the driving pivot!
public:
    TargetType type;
    double distance;
    double h_angle;
    double v_angle;
};

void DeleteTargets(vector<Target*> targets);

typedef struct PointSorterX
{
    bool operator() (Point2f pt1, Point2f pt2)
    {
        return (pt1.x < pt2.x);
    }
} PointSorterX;


void *WriteVideo(void* arg);

#endif
