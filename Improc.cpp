#include <stdio.h>
#include <pthread.h>
#include <ctime>
#include <iostream>

#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>

using namespace std;
using namespace cv;

#include "UtilityStructs.h"

#define MIN_THRESH 220
#define MAX_THRESH 255

string videoStreamAddress = "http://10.0.0.69/mjpg/video.mjpg";

void* continousFrameUpdater(void* arg)
{
  UpdaterStruct *info = (UpdaterStruct*)arg;
  Mat *criticalFrame = info->frame;
  VideoCapture *vidCap = info->vidCap;
  info->vidCap->open(videoStreamAddress);
  pthread_mutex_t *locker = info->frameLocker;

  Mat *tempMat;

  while(true)
  {
    *vidCap >> *tempMat;

    if(tempMat->empty())
    {
      cout << "Empty Frame!" << endl;
    }
    else
    {
      //Mutex Critical
      pthread_mutex_lock(locker);
      tempMat->copyTo(*criticalFrame);
      pthread_mutex_unlock(locker);
      //End of mutex critical
    }
  }
}

void* parallelGpuThreshold(void* arg)
{
  gpu::GpuMat *source_and_final = (gpu::GpuMat*)arg;
  gpu::GpuMat *middle;

  gpu::threshold(*source_and_final,*middle,MIN_THRESH,MAX_THRESH,THRESH_BINARY);
  middle->copyTo(*source_and_final);

  pthread_exit(NULL);
}

void* parallelGpuBitwise_NOT(void* arg)
{
  gpu::GpuMat *source_and_final = (gpu::GpuMat*)(arg);
  gpu::GpuMat *middle;

  gpu::bitwise_not(*source_and_final,*middle);
  middle->copyTo(*source_and_final);

  pthread_exit(NULL);
}

void PreProcessFrame(Mat *src_host, Mat *dst_host, pthread_mutex_t *frameLocker, bool DisplayResualt)
{
  Mat copiedMat;
  gpu::GpuMat src, dst, channels[3], redBlueAND, redBlueNOT, eroded;
  pthread_t thresh_threads[3],bitwise_not_threads[2];

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
    src.upload(copiedMat);
    gpu::split(src,channels);

    for(int i = 0; i < 3; i++)
    {
      pthread_create(&thresh_threads[i],NULL, parallelGpuThreshold,(void*)&channels[i]);
    }

    //wait for threshing threads to terminate
    pthread_join(thresh_threads[2],NULL);
    pthread_join(thresh_threads[0],NULL);

    gpu::bitwise_and(channels[0],channels[2],redBlueAND);
    gpu::bitwise_not(redBlueAND,redBlueNOT);

    //wait for the green threshing thread
    pthread_join(thresh_threads[1],NULL);

    gpu::bitwise_and(redBlueNOT,channels[1],dst);

    gpu::erode(dst,eroded,Mat(),Point(-1,-1),2);
    gpu::dilate(eroded,dst,Mat());

    dst.download(*dst_host);

    if(DisplayResualt)
    {
      imshow("Preprocessing Resault",*dst_host);
    }
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

  //Start processing
  while(true) //TODO loop until keypress
  {
    PreProcessFrame(&frame_host,&prePro_host,&frameLocker,true);
  }
}
