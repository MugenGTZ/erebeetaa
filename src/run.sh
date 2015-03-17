global_var="elevatorProtocol/protocol_dummy/protocol.c"
global_var1="elevatorProtocol/UDP.c"


g++ localServer/localServer.c Door.cpp Elevator.cpp HW/io.c HW/elev.c "$global_var" -o Elevator.o -lpthread -lm -lcomedi -g -Wall -lm ../../Networking/network.a ../../Networking/src/libnetcrypt.a
./Elevator.o
