#ifndef UTILITY_H_
#define UTILITY_H_

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

void DeleteTargets(vector<Target*> targets)
{
	for(int i =0;i<targets.size();i++)
		delete targets[i];
}

struct PointSorterX
{
    bool operator() (Point2f pt1, Point2f pt2)
    {
        return (pt1.x < pt2.x);
    }
} PointSorterX;

void Sort4Clockwise(vector<Point2f> &pts) //Left-right + top-down (clockwise)
{
    /*
     * Goal:
     * y
     * ↑
     * |  [0]          [1]
     * |
     * |   [3]       [2]
     * *------------------→x
     */


    //Sort points in left to right (LTR) order
    sort(pts->begin(), pts->end(), PointSorterX);

    Point2f swapper;

    /*Sort by height
     * y
     * ↑
     * |  [0]          [3]
     * |
     * |   [1]       [2]
     * *------------------→x
     */

    if(*pts[0].y < *pts[1].y)
    {
        swapper = *pts[0];
        *pts[0] = *pts[1];
        *pts[1] = swapper;
    }

    if(*pts[2].y > *pts[3].y)
    {
        swapper = *pts[0];
        *pts[0] = *pts[1];
        *pts[1] = swapper;
    }

    //Sort clockwise
    swapper = *pts[3];
    *pts[3] = *pts[1];
    *pts[1] = swapper;
}


void WriteVideo(void* info)
{   
    UpdaterStruct *info = (UpdaterStruct*)arg;
    Mat *criticalFrame = info->frame;
    VideoCapture *vcap = info->vidCap;
    pthread_mutex_t *locker = info->frameLocker;

    int frame_width = vcap.get(CV_CAP_PROP_FRAME_WIDTH);
    int frame_height = vcap.get(CV_CAP_PROP_FRAME_HEIGHT);
    VideoWriter video("out.avi",CV_FOURCC('M','J','P','G'),10, Size(frame_width,frame_height),true);

    for(;;)
    {
        Mat frame;
        
        pthread_mutex_lock(frameLocker);
        criticalFrame->copyTo(frame);
        pthread_mutex_unlock(frameLocker);

        video.write(frame);  
    }
    
    return 0;
}

#endif
