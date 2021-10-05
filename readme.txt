make.cpp:
	The make.cpp contains the source code of make application. The c++ version that used for the code is c++17.
	Compile make.cpp:
		command: g++ make.cpp -o make.exe -std=c++17 -pthread
		The -pthread option may not needed based on different environment

make.exe:
	Note the make.exe is generated on Windows 10 environment, so it may not work in Linux environment.

Makefile:
	A makefile that compile make.cpp using this command "g++ make.cpp -o make.exe -std=c++17 -pthread".
	Note the -pthread option may not needed based on different environment

test_case.zip:
	This .zip file contains the test case that used to test the make application.