SOURCE:= $(wildcard *.c) $(wildcard *.cpp)
OBJS:= $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCE)))
TARGET:= IMServer

CXX:= g++
LIBS:=`mysql_config --libs` -lcrypt
LDFLAGS:=
DEFINES:=
INCLUDE:= -I.
CXXFLAGS:= -g -Wall $(DEFINES) $(INCLUDE) `mysql_config --cflags` -std=c++11

.PHONY : all clean rebuild

all: $(TARGET)

IMServer.o: IMServer.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS) -c
main.o: main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS) -c

rebuild: clean all

clean:
	rm -f $(TARGET) $(OBJS)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)
