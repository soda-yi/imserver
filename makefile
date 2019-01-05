SOURCE:= $(wildcard *.c) $(wildcard *.cpp)
OBJS:= $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCE)))
TARGET:= imserver

CXX:= g++
LIBS:=`mysql_config --libs`
LDFLAGS:=
DEFINES:=
INCLUDE:= -I.
CXXFLAGS:= -g -Wall $(DEFINES) $(INCLUDE) `mysql_config --cflags` -std=c++11

.PHONY : all clean rebuild

all: $(TARGET)

my_mysql.o: my_mysql.cpp my_mysql.h
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS) -c
imserver.o: imserver.cpp imserver.h
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS) -c
main.o: main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS) -c

rebuild: clean all

clean:
	rm -f $(TARGET) $(OBJS)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)
