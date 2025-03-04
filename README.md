# Pomi - A GBA Pomodoro Timer

Pomi is a productivity timer for the Game Boy Advance that implements the Pomodoro Technique. Built with the Butano engine, it offers a focused, distraction-free environment for time management.

## What is the Pomodoro Technique?

The Pomodoro Technique is a time management method that uses a timer to break work into intervals (traditionally 25 minutes) separated by short breaks. After a set number of work intervals, you take a longer break. This approach helps maintain focus and manage mental fatigue.

## Features

- **Work Sessions**: Default 25-minute focused work periods
- **Break System**: Alternates between 5-minute short breaks and 15-minute long breaks
- **Session Tracking**: Counts completed work sessions
- **Configurable Timers**: Customize work and break durations
- **Visual Progress**: Shows remaining time and progress bar
- **GBA Controls**: Simple button interface

## How to Use

1. **Start/Pause**: Press A to start or pause the timer
2. **Reset**: Press B to reset the current timer
3. **Config**: Press SELECT to enter configuration mode
4. **Navigation**: Use D-pad in config mode to adjust settings

## States

- **Work**: Focus time with red progress indicator
- **Short Break**: Brief rest with green progress indicator
- **Long Break**: Extended rest with blue progress indicator
- **Config**: Adjust timer durations and sessions per set

## Building from Source

This project requires the Butano GBA engine. Follow these steps:

1. Install the Butano engine and its dependencies
2. Clone this repository
3. Run `make` in the project directory
4. Load the resulting ROM on your GBA or emulator

## License

Apache 2.0