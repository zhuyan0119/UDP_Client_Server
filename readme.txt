COEN 233 assignment1

Should run on two terminals, and run server before client.
the client will ask for the payload from user for each package. 
Three test cases will be run on the program: 
1) The client will send 5 normal packages to server, and server send back ACK for each package
2) Then the client will send 1 nomal and 4 error packages to server,  the server should reject error packages and give reject code
3) Finally the client will send 1 normal package, but the server will not response. The client will re-send the package upto 3 times.

compile c command line:     cc client.c -o client.out
run:                        ./client
