global_var="localServer.cpp"


g++ main.cpp Door.cpp Elevator.cpp HW/io.c HW/elev.c "$global_var" -o Elevator.o -lpthread -lm -lcomedi -g -Wall -lm ../../Networking/network.a ../../Networking/src/libnetcrypt.a
./Elevator.o
