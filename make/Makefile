override CXXFLAGS += -std=c++11 -lstdc++ -DGCC

ifeq "$(debug)" "1"
	override CXXFLAGS += -g
else
	override CXXFLAGS += -mavx -O2 -fstrict-aliasing -march=native -DNDEBUG
endif

override CXXFLAGS += -I../../copy_roi/copy_roi_test -D_FILE_OFFSET_BITS=64

#run: rawer 
#	sleep 0.05; \
#	./rawer /media/igor/Safari/SPIM_data/3.czi

rawer: main.o copy_roi.o
	$(CC) -o rawer $^ $(CXXFLAGS) 

main.o: ../src/main.cpp
	$(CC) $(CXXFLAGS) -c ../src/main.cpp

copy_roi.o: ../../copy_roi/copy_roi_test/copy_roi.cpp
	$(CC) $(CXXFLAGS) -c ../../copy_roi/copy_roi_test/copy_roi.cpp

clean:
	make -C ./rawer clean
	rm -f main.o

.PHONY = clean run
