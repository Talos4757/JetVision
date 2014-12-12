#include <stdio.h>
#include <pthread.h>
#include <ctime>
#include <vector>

#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace std;
using namespace cv;

#include "UtilityStructs.h"

#define MIN_THRESH 200
#define MAX_THRESH 255
#define DISPLAY true

//IP camera feed URI
const string videoStreamAddress = "http://10.0.0.69/mjpg/video.mjpg";

//This method gets the most recent fram from the camera. Runs on parallel thread!
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

    //OR the two irrelevant channels (so all the irrelevat pixels are in one Mat)
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
      imshow("Preprocessing Resault",*dst_host);
      imshow("Original Image", copiedMat);

      waitKey(1);
    }
  }
}


//CalcTargets Variables
vector<vector<Point> > contours;
vector<Vec4i> hierarchy;
Point2f rect_points[4];
Scalar purple = Scalar(255,0,255);
Scalar red = Scalar(0,0,255);
Scalar blue  = Scalar(255,0,0);
Scalar green = Scalar(0,255,0);

//This method calculates the convex hull for each target, the rotated bounding rectangle and the center of mass
void CalcTargets(Mat *src ,bool Display)
{
  //Find contours on the image
  findContours(*src, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0) );
  //Points of the convex hull
  vector<vector<Point> >hulls(contours.size());
  //Points of the bounding rectangle
  vector<RotatedRect> minRects(contours.size());

  //Calculate the convex hull and bounding rectangle from the points
  for(int i = 0; i < contours.size(); i++)
  {
    if(contours[i].size() > 15)
    {
      convexHull(Mat(contours[i]), hulls[i], false);
      minRects[i] = minAreaRect(Mat(contours[i]));
    }
  }

  // Show in a window
  if(Display)
  {
    //create an empty frame
    Mat drawing = Mat::zeros(src->size(), CV_8UC3);
    //Draw the shapes
    for( int i = 0; i< contours.size(); i++ )
    {
      drawContours(drawing, contours, i, red, 1, 8, vector<Vec4i>(), 0, Point());
      drawContours(drawing, hulls, i, purple, 1, 8, vector<Vec4i>(), 0, Point());

      minRects[i].points(rect_points);

      //Draw the center of mass of each rectangle
      circle(drawing,minRects[i].center,2,green);

      //Draw rectangles
      for(int k = 0; k < 4; k++)
      {
        line(drawing,rect_points[k], rect_points[(k+1)%4], blue, 1, 8);
      }
    }

    //Display the image
    imshow( "Raw Targets", drawing );
    waitKey(1);
  }
}

int main()
{
  //Set up the frame updaer thread
  pthread_t updater_thread;

  //Mutex setup
  pthread_mutex_t frameLocker;
  pthread_mutex_init(&frameLocker,NULL);

  //video capture setup
  VideoCapture vidCap;
  Mat frame_host;
  Mat prePro_host;

  //Parallel frame updater info struct
  UpdaterStruct frameUpdaterInfo;
  frameUpdaterInfo.vidCap = &vidCap;
  frameUpdaterInfo.frame = &frame_host;
  frameUpdaterInfo.frameLocker = &frameLocker;

  //Start the updater thread
  pthread_create(&updater_thread,NULL,continousFrameUpdater,(void*)&frameUpdaterInfo);

  //Clock variables
  struct timespec start, finish;
  double elapsed;
  double fps;

  while(true) //TODO loop until keypress
  {
    //Start the clock
    clock_gettime(CLOCK_MONOTONIC, &start);

    //Start processing
    PreProcessFrame(&frame_host,&prePro_host,&frameLocker,DISPLAY);

    if(!prePro_host.empty())
    {
      CalcTargets(&prePro_host,DISPLAY);

      //Clock
      clock_gettime(CLOCK_MONOTONIC, &finish);
      elapsed = (finish.tv_sec - start.tv_sec);
      elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000.0;
      fps = 1 / (elapsed / 1000.0);
      cout <<  fps  << " FPS" << endl;
    }
  }
}
