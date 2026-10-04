g++ main.cpp src\*.cpp -o fish.exe -std=c++17 -pthread -static -O2 -mwindows -Isrc -lgdi32 -lgdiplus -lwinhttp -limm32
pause
