#include <stdio.h>
#include <iostream>
#include <pthread.h>

#include <ctime>

#include <opencv2/opencv.hpp>
#include <opencv2/gpu/gpu.hpp>

using namespace cv;
using namespace std;

#define MIN_THRESH 220
#define MAX_THRESH 255

//GLOBALS
VideoCapture vcap;
Mat frame;
pthread_mutex_t frameLocker;

const string videoStreamAddress = "http://10.0.0.69/mjpg/video.mjpg";

//Frame updater method which runs on seperate thread
void *UpdateFrame(void *arg)
{
	for(;;)
	{
		Mat tempFrame;
		vcap >> tempFrame;
		
		//Mutex critial area
		pthread_mutex_lock(&frameLocker);
		frame = tempFrame;
		pthread_mutex_unlock(&frameLocker);
		//End of MUTEX critical area
	}
}

int main(int, char**) {	

	//Open the video stream 
	vcap.open(videoStreamAddress);
	
	//initialize threads
	pthread_mutex_init(&frameLocker,NULL);	
	pthread_t UpdThread;	
	pthread_create(&UpdThread, NULL, UpdateFrame, NULL);

	//Clock variables
	struct timespec start, finish;
	double elapsed;

	//Matrices
	Mat currentFrame, threshed, redblue;
	gpu::GpuMat dst, src, chan[3], ths[3], redAndBlue, erd;

	for(;;)
	{
		//Start the clock
		clock_gettime(CLOCK_MONOTONIC, &start);

		//MUTEX critical area
		pthread_mutex_lock(&frameLocker);
		currentFrame = frame;
		pthread_mutex_unlock(&frameLocker);
		//End of Mutex critial area

		if(currentFrame.empty()){
			cout << "Recieved empty frame" << endl;
			continue;
		}

		//GPU Code
		src.upload(currentFrame);

		gpu::split(src,chan); //Split to three channels

		gpu::threshold(chan[1],ths[1],MIN_THRESH,MAX_THRESH,THRESH_BINARY); //threshold green

		//Threshold the two other channels
		gpu::threshold(chan[0],ths[0],MIN_THRESH,MAX_THRESH,THRESH_BINARY);
		gpu::threshold(chan[2],ths[2],MIN_THRESH,MAX_THRESH,THRESH_BINARY);

		//Invert the Red and Blue Channels
		gpu::bitwise_not(ths[0],chan[0]);
		gpu::bitwise_not(ths[2],chan[2]);

		//AND it all togther
		gpu::bitwise_and(chan[0],chan[2],redAndBlue);
		gpu::bitwise_and(redAndBlue,ths[1],dst);

		//Filter out little pseudo targets
		gpu::erode(dst,erd,Mat());
		gpu::dilate(erd,dst,Mat());

		//Download to RAM
		dst.download(threshed);
//		redAndBlue.download(redblue);

		//imshow - disabled for performance
//		imshow("Original Image", currentFrame);
//		imshow("NOT Red AND NOT Blue", redblue);
//		imshow("Threshed", threshed);

		//Clock
		clock_gettime(CLOCK_MONOTONIC, &finish);
		elapsed = (finish.tv_sec - start.tv_sec);
		elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000.0;
		cout << elapsed << "ms" << endl;

		waitKey(1);
	}
}


