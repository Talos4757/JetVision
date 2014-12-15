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

#define MIN_THRESH 180
#define MAX_THRESH 255
#define DISPLAY true //change to false in production code!

#define FOV 67
const int H_RES = 640;
const int  V_RES = 480;
const double pixel_multi = 0.08375;
//Formula: (FOV / (sqrt(pow(H_RES,2)+pow(V_RES,2))))

//Some colors
Scalar purple = Scalar(255,0,255);
Scalar red = Scalar(0,0,255);
Scalar blue  = Scalar(255,0,0);
Scalar green = Scalar(0,255,0);
Scalar yellow = Scalar(0,225,255);

//IP camera feed URI (TODO get this from params)
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
Point2f rect_points[4];
double width, height, area_ratio, ratio, h_angle, v_angle;
Mat drawing;

//This method identifies targets and calculates distance and angles to each target
vector<Target> CalcTargets(Mat *src ,bool Display)
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

  vector<Target> targets;

  //Verify targets
  for(int i = 0; i< contours.size(); i++ )
  {
    height = min(minRects[i].size.height,minRects[i].size.width);
    width = max(minRects[i].size.height,minRects[i].size.width);
    ratio = width / height;
    area_ratio = (width * height) / contourArea(contours[i]);

    /* Target verification by testing width to height ratio and confirming that 
     * the target is close to rectangle shape
     */
    if(ratio > 3 && ratio < 8 && area_ratio > 0.9)
    {
      minRects[i].points(rect_points);

      h_angle = pixel_multi*(minRects[i].center.x-(H_RES/2));
      v_angle = pixel_multi*(minRects[i].center.y-(V_RES/2));

      cout << "Horizontal: " << h_angle << " Vertical: " << v_angle << endl;

      //Add target to the vector
      //TODO Add other data!
      Target currentTarget;
      currentTarget.v_angle = v_angle;
      currentTarget.h_angle = h_angle;

      //Ad target to out vector
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

  vector<Target> targets;

  while(true) //TODO loop until keypress
  {
    //Start the clock
    clock_gettime(CLOCK_REALTIME, &start);

    //Start processing
    PreProcessFrame(&frame_host,&prePro_host,&frameLocker,DISPLAY);

    if(!prePro_host.empty())
    {
      targets = CalcTargets(&prePro_host,DISPLAY);
      //TODO SendData(targets);

      //Clock
      clock_gettime(CLOCK_REALTIME, &finish);
      elapsed = (finish.tv_sec - start.tv_sec);
      elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000.0;
      fps = 1 / (elapsed / 1000.0);
//    cout <<  fps  << " FPS" << endl;
    }
  }
}
