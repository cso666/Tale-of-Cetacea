# Tale of Cetacea

A desktop whale-girl companion powered by DeepSeek, featuring affection-based visual states, dialogue memory, file feeding, and a lightweight Win32 renderer.

Note: API_key.txt is empty. If you want to talk with Cetacea, you need to enter your own API key.

Repository Structure

blink/ Idle blinking animation frames

sit/ Sitting animation frames

eatfile/ Eating animation frames

token/ Special animation when fed a token file

input/ Input box background image

output/ Speech bubble background image

main.cpp Main source code

compile.bat Build script

close.bat Kill the running process

fish.exe Prebuilt executable

fish.png Icon / reference image

Personality.txt Character card

API_key.txt Empty by default. Put your own key here.

Build

With MinGW-w64:

g++ main.cpp -o fish.exe -lgdi32 -lgdiplus -lwinhttp -limm32 -mwindows -static -static-libgcc -static-libstdc++

Or just run compile.bat.

Setup

Open API_key.txt and paste your DeepSeek API key.

Make sure all asset folders are in the same directory as fish.exe.

Run fish.exe.

Runtime Data

Conversation history and affection value are saved to:

archive/history.txt
archive/favor.txt

You can delete or edit them to reset progress.

Features

Transparent, always-on-top Win32 window

Idle, sitting, blinking and eating animations

Drag and drop file feeding

Special reaction when fed a file containing "token"

Chat input with DeepSeek API

Conversation history and automatic summarization

Affection system driven by AI judgement

Affection-based visual states

Notes

Do not commit a real API key to a public repository.

This project is for personal and non-commercial use.

The character artwork may have its own license. Check before redistributing.