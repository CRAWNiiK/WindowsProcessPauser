# Process Pauser
![Screenshot of Process Pauser](images/screen.png)
## Overview

**Process Pauser** is a Windows application that allows users to pause processes by their name (e.g., `GTA5.exe`). The user can specify the time duration for which the process will be paused. The application provides a simple graphical user interface (GUI) for controlling the pausing of processes, along with a progress bar that shows the countdown.

## Intended Purpose

In GTA Online and Red Dead Online, if you suspend your game process for 10 seconds, it will seemingly "boot" everyone from your lobby and give you a fresh public lobby. 
I also found out that when you are stuck in the clouds in GTAO trying to join a game, if you use the **Process Pauser**, it will load you in much quicker. But it will still be a solo public lobby. 

## Features

- **Pause/Resume Process**: Suspend processes with a simple button click.
- **Progress Bar**: A progress bar showing the remaining time until the process is resumed.
- **UI Controls**: Editable text fields for specifying the process name and the time to pause.

## Requirements

- **Windows OS**: Windows 7 or newer (tested on Windows 11)
- **Libraries**:
  - `windows.h`
  - `tlhelp32.h`
  - `commctrl.h` (for the progress bar and custom button drawing)

## How to Compile

1. Install MinGW
2. Open CMD and navigate to the directory containing main.cpp
3. `g++ main.cpp resource.o -o ProcessPauser.exe -mwindows -lcomctl32`

or

1. Open the script in your preferred C++ IDE (e.g., Visual Studio, Code::Blocks).
2. Ensure that `comctl32.lib` is linked (needed for custom control elements).
3. Compile and run the application.

## Usage

1. **Process Name**: Enter the name of the process you want to suspend (e.g., `GTA5.exe`).
2. **Pause Time**: Enter the number of seconds you want the process to be paused. The time is limited to a range between 1 and 999 seconds.
3. **Pause Button**: Click the "Pause" button to suspend the process. The button text will change to "Suspending" and the progress bar will start filling.
4. **Close Button**: The "X" button will close the application window. If the process is paused, it will be resumed before closing.

## Customization

- **UI Color Scheme**: The default background color for the window and controls is set to dark shades. You can change this by modifying the `RGB` values in the code.
- **Button Styling**: Custom styles for buttons are defined in the `DrawButton` function, where the button's pressed and unpressed colors can be customized.

## Known Issues

- The application only works with processes that the current user has permission to control (i.e., processes running under the same user account or with sufficient privileges).
- If a process is terminated while paused, the application will notify the user and reset the UI.

## License

This project has no license, do with it what you want.

## Made By

**CRAWNiiK** - 2024

