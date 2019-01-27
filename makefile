SOURCE:= $(wildcard *.c) $(wildcard *.cpp)
OBJS:= $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCE)))
TARGET:= imserver

CXX:= g++
LIBS:=
LDFLAGS:=
DEFINES:=
INCLUDE:= -I.
CXXFLAGS:= -g -Wall $(DEFINES) $(INCLUDE) -std=c++11 `mysql_config --cflags`

.PHONY : all clean rebuild

all: $(TARGET)

message.o: message.cpp message.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS) -c
select.o: select.cpp select.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS) -c
socket.o: socket.cpp socket.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS) -c
utility.o: utility.cpp utility.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS) -c
mysql.o: mysql.cpp mysql.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS) -c
imserver.o: imserver.cpp imserver.hpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS) -c
main.o: main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) $(LIBS) -c

rebuild: clean all

clean:
	rm -f $(TARGET) $(OBJS)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS) `mysql_config --libs`
