#include <stdio.h>
#include <pthread.h>
#include <ctime>
#include <vector>

#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "Utility.h"
#include "JetClient.h"

using namespace std;
using namespace cv;

//Clock constants
#define USED_CLOCK CLOCK_MONOTONIC_RAW
#define NANOS 1000000000LL

//Threshold values
const int MIN_THRESH = 180;
const int MAX_THRESH  = 255;

//Field of View (FOV) constants
const int H_FOV = 67;
const int V_FOV = 51;
const int H_RES = 640;
const int  V_RES = 480;
const double h_pixel_multi = 0.1046875; //67/640
const double v_pixel_multi = 0.10625; //51/480

//Target constants - TEST VALUES
const int h_pix = 200; //Pixels seen from one meter when centralized to the target
const double actual_h = 2.00; //Meter

const int v_pix = 100; //Pixels seen from one meter when centralized to the target
const double actual_v = 1.00; //Meter

const int pixel_meter = v_pix/actual_v; //should be the same for vertical and horizontal

//Colors
Scalar purple = Scalar(255,0,255);
Scalar red = Scalar(0,0,255);
Scalar blue  = Scalar(255,0,0);
Scalar green = Scalar(0,255,0);
Scalar yellow = Scalar(0,225,255);

//Default camera URI and display parameter
string videoStreamAddress = "http://192.168.0.69/mjpg/video.mjpg";
bool DISPLAY = false;


//This method gets the most recent frame from the camera. Runs on parallel thread!
void* continousFrameUpdater(void* arg)
{
    //Convert everything back from void*
    UpdaterStruct *info = (UpdaterStruct*)arg;
    Mat *criticalFrame = info->frame;
    VideoCapture *vidCap = info->vidCap;
    info->vidCap->open(videoStreamAddress);
    pthread_mutex_t *locker = info->frameLocker;

    //A temporary Mat to store the frame until the mutex is unlocked
    Mat tempMat;

    while(true)
    {
        *vidCap >> tempMat;

        if(!tempMat.empty())
        {
            //Mutex Critical
            pthread_mutex_lock(locker);
            tempMat.copyTo(*criticalFrame);
            pthread_mutex_unlock(locker);
            //End of mutex critical
        }
    }
}

//PreProcessFrame variables. It is faster to declare them once, outside the method
Mat copiedMat;
gpu::GpuMat src, dst, channels[3],midChannels[3], redBlueAND, redBlueNOT, eroded;

//This method keeps only the very green pixels (aka target) in the raw image from the camera
void PreProcessFrame(Mat *src_host, Mat *dst_host, pthread_mutex_t *frameLocker, bool DisplayResualt)
{
    //Mutex critical area
    pthread_mutex_lock(frameLocker);
    src_host->copyTo(copiedMat);
    pthread_mutex_unlock(frameLocker);
    //End of mutex critical area

    if(copiedMat.empty())
    {
        cout << "Cannot process an empty frame. Skipping" << endl;
    }
    else
    {
        //Upload to the GPU memory for processing there
        src.upload(copiedMat);

        //Split the frame into three channels: Red, Blue and Green
        gpu::split(src,midChannels);

        //Threshold all the channels
        for(int i = 0; i < 3; i++)
        {
            gpu::threshold(midChannels[i],channels[i],MIN_THRESH,MAX_THRESH,THRESH_BINARY);
        }

        //OR the two irrelevant channels (so all the irrelevant pixels are in one Mat)
        gpu::bitwise_or(channels[0],channels[2],redBlueAND);

        //Make a bitwise NOT on the OR of the Red and Blue channels,...
        gpu::bitwise_not(redBlueAND,redBlueNOT);
        //...So when we AND them with the green channel only the green targets are left (and not white ones, for example)
        gpu::bitwise_and(redBlueNOT,channels[1],dst);

        //Get rid of really small pixels
        gpu::erode(dst,eroded,Mat(),Point(-1,-1),3);
        gpu::dilate(eroded,dst,Mat(),Point(-1,-1),3);

        //Download the frame from the GPU memory to the CPU memory (RAM) so we can use it later
        dst.download(*dst_host);

        if(DisplayResualt)
        {
            //Show the images
            imshow("Preprocessing Result",*dst_host);
            imshow("Original Image", copiedMat);

            waitKey(1);
        }
    }
}


//CalcTargets Variables
vector<vector<Point> > contours;
Point2f rect_points[4];
vector<Point2f> corners;
double width, height, area_ratio, ratio, h_angle, v_angle, dist;
int raw_R, raw_L;
Mat drawing;

//This method identifies targets and calculates distance and angles to each target
vector<Target*> CalcTargets(Mat *src ,bool Display)
{
    //create an empty frame
    if(Display)
    {
        drawing = Mat::zeros(src->size(), CV_8UC3);
        circle(drawing,Point(H_RES/2,V_RES/2),2,yellow);
    }

    //Find contours on the image. (external contours only)
    findContours(*src, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

    //Points of the bounding rectangle
    vector<RotatedRect> minRects(contours.size());

    //Calculate the rotated bounding rectangle from the points
    for(int i = 0; i < contours.size(); i++)
    {
        if(contours[i].size() > 15)
        {
            minRects[i] = minAreaRect(Mat(contours[i]));
        }
    }

    vector<Target*> targets;

    for(int i = 0; i < contours.size() - 1; i++ )
    {
        Target *currentTarget = new Target();

        minRects[i].points(rect_points);

        // TODO this should use the center of mass of the contour and not the bounding rectangle
        h_angle = h_pixel_multi*(minRects[i].center.x-(H_RES/2));
        v_angle = v_pixel_multi*(minRects[i].center.y-(V_RES/2));
        h_angle += h_pixel_multi*(minRects[i + 1].center.x-(H_RES/2));
        v_angle += v_pixel_multi*(minRects[i + 1].center.y-(V_RES/2));

        h_angle /= 2;
        v_angle /= 2;

        cout << "Horizontal: " << h_angle << " Vertical: " << v_angle << " Distance:" << dist << endl;

        //Add target to the vector                
        currentTarget->v_angle = v_angle;
        currentTarget->h_angle = h_angle;
        currentTarget->distance = 0;
        targets.push_back(currentTarget);

        if(Display)
        {
            //Draw the center of mass of each rectangle
            circle(drawing,minRects[i].center,2,green);

            //Draw rectangles
            for(int k = 0; k < 4; k++)
            {
                line(drawing,rect_points[k], rect_points[(k+1)%4], green, 1, 8);
            }
        }    

        
        //Draw the contours
        if(Display)
        {
            drawContours(drawing, contours, i, red);

            //Display the image
            imshow( "Targets", drawing);
            waitKey(1);
        }
    }

    return targets;
}

int main(int argc, char* argv[])
{
    cout << "Starting the camera capture thread..." << endl;
    
    //Set up the frame updater thread
    pthread_t updater_thread;

    //Mutex setup
    pthread_mutex_t frameLocker;
    pthread_mutex_init(&frameLocker,NULL);

    //video capture setup
    VideoCapture vidCap;
    Mat frame_host;
    Mat preProcessing_host;

    //Parallel frame updater info struct
    UpdaterStruct frameUpdaterInfo;
    frameUpdaterInfo.vidCap = &vidCap;
    frameUpdaterInfo.frame = &frame_host;
    frameUpdaterInfo.frameLocker = &frameLocker;

    //Start the updater thread
    pthread_create(&updater_thread,NULL,continousFrameUpdater,(void*)&frameUpdaterInfo);

    //Parse arguments
    if(argc > 1)
    {
        for(int i = 1; i < argc; i++)
        {
            if(string(argv[i]) == "--enable-gui")
            {
                DISPLAY = true;
            }
            
            if(string(argv[i]) == "--address")
            {
                videoStreamAddress = "http://" + string(argv[i+1]) + "/mjpg/video.mjpg";
            }            
        }
    }

    
	cout << "Server Initializing.." <<endl;
    if(JetClient::Init())
    {
		cout << "Client Connected" << endl;
		
        //Clock variables
        double fps;
        struct timespec begin, current;
        long long start, elapsed, microseconds;

        while(true)
        {
            //Start the clock
            clock_gettime(USED_CLOCK, &begin);
            start = begin.tv_sec*NANOS + begin.tv_nsec;

            //Pre-process the frame (isolate targets and convert to black and white)
            PreProcessFrame(&frame_host,&preProcessing_host,&frameLocker,DISPLAY);

            if(!preProcessing_host.empty())
            {
				//Calculate the targets angle and distance
				vector<Target*> targets = CalcTargets(&preProcessing_host,DISPLAY);

				//Send the info to the c/RoboRIO
                if(JetClient::SendTargets(targets) == false)
                {
                    cout << "Error while sending target data" << endl;
                }
				
				DeleteTargets(targets);

                //Clock
                clock_gettime(USED_CLOCK, &current);
                elapsed = current.tv_sec*NANOS + current.tv_nsec - start;
                fps = 1 / (elapsed / 1000000000.0);
                cout <<  fps  << " FPS" << endl;
            }
        }
    }
    else
    {
        cout << "Could not connect to server, aborting." << endl;
    }
}
