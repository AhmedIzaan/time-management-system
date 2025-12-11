# Multi-Process & Multi-Threaded Time Management System

## Project Overview
A comprehensive Time Management System implementing core Operating Systems concepts including multi-threading, multi-processing, synchronization primitives (mutexes and semaphores), and inter-process communication (IPC).

## Features Implemented

### 1. **Live Clock (Thread-based)** ✓
- Runs continuously in background thread
- Displays time in 12-hour format 
- Uses mutex for thread-safe updates to shared display string
- Updates every 500ms

### 2. **Alarm System (Separate Process)** ✓
- Managed by forked child process
- Parent sends alarm times to child through pipe
- Child checks time every second
- When triggered, writes back to parent using pipe
- Parent displays "ALARM RINGING!!!" message
- Implements Producer-Consumer pattern with semaphores

### 3. **Stopwatch (Thread-based)** ✓
- Start/Resume functionality (Press 'S')
- Pause functionality (Press 'P')
- Reset functionality (Press 'R')
- Displays time in HH:MM:SS format
- Mutex prevents race conditions with shared stopwatch data
- Supports pause/resume without losing elapsed time

### 4. **Countdown Timer (Thread-based)** ✓
- User can set multiple countdown durations:
  - 30 seconds (Press 'T')
  - 1 minute (Press '1')
  - 2 minutes (Press '2')
  - 5 minutes (Press '3')
- Stop timer (Press 'O')
- Displays "TIMER DONE!" when countdown reaches zero
- Mutex synchronization prevents conflicts with other threads

### 5. **Menu-Based Navigation (Parent Process)** ✓
- Interactive keyboard-based menu system
- Clear on-screen instructions
- Real-time response to user input
- Controls all system components

## OS Concepts Applied

### Process Management
- **Process Creation**: `fork()` creates child process for alarm system
- **Process Isolation**: Child and parent have separate memory spaces
- **Process Communication**: Bidirectional pipes for IPC

### Thread Management
- **pthread Creation**: Three background threads (clock, stopwatch, timer)
- **Thread Lifecycle**: Proper thread join on cleanup
- **Atomic Operations**: `std::atomic<bool>` for thread control flags

### Synchronization
- **Mutexes**: 
  - `displayMutex`: Protects live clock string
  - `stopwatchMutex`: Protects stopwatch data
  - `timerMutex`: Protects countdown timer data
  - `alarmMutex`: Protects alarm status
- **Semaphores**: 
  - `alarmSemaphore`: Coordinates alarm producer-consumer pattern
- **Critical Sections**: All shared data access wrapped in mutex locks
- **Race Condition Prevention**: Proper locking order and scope

### Inter-Process Communication
- **Unnamed Pipes**: 
  - Parent-to-Child pipe: Sends alarm times
  - Child-to-Parent pipe: Sends alarm notifications
- **Non-blocking I/O**: Parent pipe read is non-blocking to prevent GUI freeze

### Concurrency Patterns
- **Producer-Consumer**: Alarm system uses semaphores
- **Reader-Writer**: Multiple threads read shared time data
- **Thread-Safe Updates**: Lock guards ensure atomic updates

## Compilation & Execution

### Prerequisites
```bash
sudo apt update
sudo apt install build-essential libsfml-dev
```

### Compile
```bash
cd OS_Time_System
make
```

### Run
```bash
make run
# or
./build/time_app
```

### Clean
```bash
make clean
```

## User Controls

| Key | Function |
|-----|----------|
| S | Start/Resume Stopwatch |
| P | Pause Stopwatch |
| R | Reset Stopwatch |
| T | Start 30-second Timer |
| 1 | Start 1-minute Timer |
| 2 | Start 2-minute Timer |
| 3 | Start 5-minute Timer |
| O | Stop Timer |
| A | Set Alarm (10 seconds from now) |
| ESC | Exit Application |

## Architecture

```
┌─────────────────────────────────────────────┐
│         PARENT PROCESS (Main/GUI)           │
│                                             │
│  ┌─────────────┐  ┌──────────────┐        │
│  │ Clock Thread│  │Stopwatch Thrd│        │
│  └──────┬──────┘  └──────┬───────┘        │
│         │                │                 │
│    ┌────▼────────────────▼──┐             │
│    │    displayMutex        │             │
│    │  stopwatchMutex        │             │
│    │    timerMutex          │             │
│    └────────────────────────┘             │
│                                             │
│         │ Pipe                              │
│         ▼                                   │
└─────────┼───────────────────────────────────┘
          │
┌─────────▼───────────────────────────────────┐
│         CHILD PROCESS (Alarm System)        │
│                                             │
│  - Receives alarm times via pipe            │
│  - Checks system time every second          │
│  - Sends notification when alarm triggers   │
└─────────────────────────────────────────────┘
```

## Technical Details

### Thread Safety
- All shared variables are protected by mutexes
- `std::lock_guard` ensures exception-safe locking
- Atomic flags for thread control prevent data races


### Process Management
- Child process properly forked and managed
- SIGKILL sent to child on parent exit
- Pipe file descriptors properly closed

### GUI Framework
- SFML for cross-platform graphics
- Event-driven architecture
- Non-blocking operations preserve responsiveness

## Files Structure
```
OS_Time_System/
├── build/
│   └── time_app          # Compiled executable
├── src/
│   └── main.cpp          # Complete source code
├── Makefile              # Build configuration
└── README.md             
```

## Academic Learning Outcomes

This project demonstrates mastery of:
1. Process vs Thread distinction and use cases
2. Critical section identification and protection
3. Deadlock-free synchronization design
4. IPC mechanisms (pipes)
5. Producer-consumer problem solving
6. Race condition prevention
7. Multi-threaded application architecture
8. Resource cleanup and lifecycle management

## Compliance with Lab Requirements

✅ Multi-process architecture (fork)  
✅ Multi-threaded design (3 worker threads)  
✅ Mutexes for synchronization  
✅ Semaphores for producer-consumer  
✅ Inter-process communication (pipes)  
✅ Live clock with 12-hour format  
✅ Alarm system in separate process  
✅ Stopwatch with start/stop/resume/reset  
✅ Countdown timer  
✅ Menu-based navigation  
✅ Critical section protection  
✅ Race condition prevention  

## Author
Ahmed Izaan
