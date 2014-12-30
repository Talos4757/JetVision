CC = g++
CPPS := $(wildcard src/*.cpp) 
TARGET = bin/Improc
LIBS = -lopencv_core -lopencv_imgproc -lopencv_highgui\
 -lopencv_calib3d -lopencv_contrib -lopencv_features2d\
 -lopencv_flann -lopencv_gpu -lopencv_legacy -lopencv_ml\
 -lopencv_nonfree -lopencv_objdetect -lopencv_photo\
 -lopencv_stitching -lopencv_superres -lopencv_video\
 -lopencv_videostab -lpthread
CCARGS = -std=c++11

all : $(TARGET)

$(TARGET) : $(CPPS)
	$(CC) $(CCARGS) $(CPPS) $(LIBS) $(OBJECTS) -o $@

.PHONY : clean
clean :
	rm -rf src/*.o
