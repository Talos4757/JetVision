#include <opencv2/opencv.hpp>
#include "Utility.h"

using namespace std;
using namespace cv;

void DeleteTargets(vector<Target*> targets)
{
	for(int i =0;i<targets.size();i++)
		delete targets[i];
}

void *WriteVideo(void* arg)
{   
    UpdaterStruct *info = (UpdaterStruct*)arg;
    Mat *criticalFrame = info->frame;
    VideoCapture *vcap = info->vidCap;
    pthread_mutex_t *locker = info->frameLocker;

    int frame_width = vcap->get(CV_CAP_PROP_FRAME_WIDTH);
    int frame_height = vcap->get(CV_CAP_PROP_FRAME_HEIGHT);
    VideoWriter video("out.avi",CV_FOURCC('M','J','P','G'),10, Size(frame_width,frame_height),true);

    for(;;)
    {
        Mat frame;
        
        pthread_mutex_lock(locker);
        criticalFrame->copyTo(frame);
        pthread_mutex_unlock(locker);

        video.write(frame);  
    }
    
    return 0;
}