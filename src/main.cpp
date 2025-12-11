#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <semaphore.h>
#include <chrono>
#include <ctime>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <iomanip>
#include <vector>
#include <signal.h>

using namespace std;

// ================= GLOBAL SHARED RESOURCES =================

// 1. MUTEXES: Protect shared data between Threads
mutex displayMutex;
mutex stopwatchMutex;
mutex timerMutex;
mutex alarmMutex;
mutex controlMutex;

// 2. SHARED DATA
string sharedTimeStr = "00:00:00";
string stopwatchStr = "00:00:00";
string timerStr = "00:00";
string alarmStatus = "No Alarm Set";

// 3. THREAD CONTROL (using bool with mutex instead of atomic)
bool isRunning = true;
bool stopwatchRunning = false;
bool stopwatchPaused = false;
bool timerRunning = false;
bool timerPaused = false;

// 4. STOPWATCH DATA
int stopwatchHours = 0, stopwatchMinutes = 0, stopwatchSeconds = 0;

// 5. TIMER DATA
int timerMinutes = 0, timerSeconds = 0;

// 6. ALARM DATA
vector<string> alarmTimes;
bool alarmRinging = false;

// 7. SEMAPHORE for Producer-Consumer pattern
sem_t alarmSemaphore;

// ================= WORKER THREAD: LIVE CLOCK =================
void clockThreadFunc() {
    bool running = true;
    while (running) {
        // Get System Time
        time_t t = time(nullptr);
        tm* now = localtime(&t);

        // Format Time (12-hour format with AM/PM)
        int hour12 = now->tm_hour % 12;
        if (hour12 == 0) hour12 = 12; // Convert 0 to 12 for midnight/noon
        string ampm = (now->tm_hour >= 12) ? "PM" : "AM";
        
        stringstream ss;
        ss << setfill('0') << setw(2) << hour12 << ":"
           << setfill('0') << setw(2) << now->tm_min << ":"
           << setfill('0') << setw(2) << now->tm_sec << " " << ampm;

        // CRITICAL SECTION: Lock before writing to shared string
        {
            lock_guard<mutex> lock(displayMutex);
            sharedTimeStr = ss.str();
        }
        
        // Check if still running
        {
            lock_guard<mutex> lock(controlMutex);
            running = isRunning;
        }

        this_thread::sleep_for(chrono::milliseconds(500));
    }
}

// ================= WORKER THREAD: STOPWATCH =================
void stopwatchThreadFunc() {
    auto startTime = chrono::steady_clock::now();
    bool running = true;
    
    while (running) {
        bool swRunning, swPaused;
        {
            lock_guard<mutex> lock(controlMutex);
            swRunning = stopwatchRunning;
            swPaused = stopwatchPaused;
            running = isRunning;
        }
        
        if (swRunning && !swPaused) {
            auto currentTime = chrono::steady_clock::now();
            auto elapsed = chrono::duration_cast<chrono::seconds>(
                currentTime - startTime).count();
            
            // CRITICAL SECTION: Lock before updating stopwatch data
            {
                lock_guard<mutex> lock(stopwatchMutex);
                stopwatchHours = elapsed / 3600;
                stopwatchMinutes = (elapsed % 3600) / 60;
                stopwatchSeconds = elapsed % 60;
                
                stringstream ss;
                ss << setfill('0') << setw(2) << stopwatchHours << ":"
                   << setfill('0') << setw(2) << stopwatchMinutes << ":"
                   << setfill('0') << setw(2) << stopwatchSeconds;
                stopwatchStr = ss.str();
            }
        } else if (swPaused) {
            // When paused, update start time to maintain current elapsed time
            auto currentTime = chrono::steady_clock::now();
            int totalSeconds = stopwatchHours * 3600 + stopwatchMinutes * 60 + stopwatchSeconds;
            startTime = currentTime - chrono::seconds(totalSeconds);
        }
        
        this_thread::sleep_for(chrono::milliseconds(100));
    }
}

// ================= WORKER THREAD: COUNTDOWN TIMER =================
void timerThreadFunc() {
    bool running = true;
    
    while (running) {
        bool tmRunning, tmPaused;
        {
            lock_guard<mutex> lock(controlMutex);
            tmRunning = timerRunning;
            tmPaused = timerPaused;
            running = isRunning;
        }
        
        if (tmRunning && !tmPaused) {
            this_thread::sleep_for(chrono::seconds(1));
            
            // CRITICAL SECTION: Lock before updating timer
            {
                lock_guard<mutex> lock(timerMutex);
                
                if (timerSeconds > 0) {
                    timerSeconds--;
                } else if (timerMinutes > 0) {
                    timerMinutes--;
                    timerSeconds = 59;
                } else {
                    // Timer finished
                    lock_guard<mutex> ctrlLock(controlMutex);
                    timerRunning = false;
                    timerStr = "TIMER DONE!";
                    continue;
                }
                
                stringstream ss;
                ss << setfill('0') << setw(2) << timerMinutes << ":"
                   << setfill('0') << setw(2) << timerSeconds;
                timerStr = ss.str();
            }
        } else {
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }
}

// ================= CHILD PROCESS: ALARM SYSTEM =================
// This runs as a completely separate process (Forked).
// It does NOT share memory with main, so it communicates via PIPE.
void runAlarmProcess(int readPipeFd, int writePipeFd) {
    // Set read pipe to non-blocking
    fcntl(readPipeFd, F_SETFL, O_NONBLOCK);
    
    vector<string> childAlarmTimes;
    char buffer[256];
    
    while (true) {
        // Check for new alarm times from parent (non-blocking read)
        ssize_t bytesRead = read(readPipeFd, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            string newAlarm(buffer);
            if (newAlarm.length() == 8) { // Valid time format HH:MM:SS
                childAlarmTimes.push_back(newAlarm);
            }
        }
        
        // Get current time
        time_t t = time(nullptr);
        tm* now = localtime(&t);
        
        char currentTime[9];
        snprintf(currentTime, sizeof(currentTime), "%02d:%02d:%02d", 
                 now->tm_hour, now->tm_min, now->tm_sec);
        
        // Check if any alarm matches current time
        for (size_t i = 0; i < childAlarmTimes.size(); i++) {
            if (childAlarmTimes[i] == string(currentTime)) {
                char msg = 'A'; // 'A' for Alarm
                write(writePipeFd, &msg, 1);
                // Remove triggered alarm
                childAlarmTimes.erase(childAlarmTimes.begin() + i);
                break;
            }
        }
        
        sleep(1);
    }
}

// ================= MAIN PROCESS: CONTROLLER & UI =================
int main() {
    // 1. INITIALIZE SEMAPHORE
    sem_init(&alarmSemaphore, 0, 1);
    
    // 2. SETUP PIPES (Inter-Process Communication)
    int pipeParentToChild[2]; // Parent writes alarm times to child
    int pipeChildToParent[2]; // Child writes alarm notifications to parent
    
    if (pipe(pipeParentToChild) == -1 || pipe(pipeChildToParent) == -1) {
        perror("Pipe failed");
        return 1;
    }

    // Make the Read end NON-BLOCKING so it doesn't freeze the GUI
    fcntl(pipeChildToParent[0], F_SETFL, O_NONBLOCK);

    // 3. FORK PROCESS
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        return 1;
    }

    // --- CHILD PROCESS CODE ---
    if (pid == 0) {
        close(pipeParentToChild[1]); // Close unused write end
        close(pipeChildToParent[0]); // Close unused read end
        
        // Detach from parent's SFML resources
        runAlarmProcess(pipeParentToChild[0], pipeChildToParent[1]);
        
        close(pipeParentToChild[0]);
        close(pipeChildToParent[1]);
        _exit(0); // Use _exit to avoid flushing parent's buffers
    }

    // --- PARENT PROCESS CODE (GUI) ---
    close(pipeParentToChild[0]); // Close unused read end
    close(pipeChildToParent[1]); // Close unused write end

    // 4. START THREADS
    thread bgClock(clockThreadFunc);
    thread bgStopwatch(stopwatchThreadFunc);
    thread bgTimer(timerThreadFunc);

    // 5. SFML WINDOW SETUP
    sf::RenderWindow window(sf::VideoMode(900, 700), "OS Lab: Time Management System");
    
    sf::Font font;
    if (!font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf")) {
        std::cerr << "Font not found. Install fonts-dejavu or change path." << std::endl;
        return -1;
    }

    // Title
    sf::Text titleText;
    titleText.setFont(font);
    titleText.setCharacterSize(32);
    titleText.setFillColor(sf::Color::Yellow);
    titleText.setPosition(200, 20);
    titleText.setString("Time Management System");

    // Live Clock Label
    sf::Text clockLabel;
    clockLabel.setFont(font);
    clockLabel.setCharacterSize(20);
    clockLabel.setFillColor(sf::Color::Cyan);
    clockLabel.setPosition(50, 80);
    clockLabel.setString("Live Clock:");

    // Live Clock Display
    sf::Text timeText;
    timeText.setFont(font);
    timeText.setCharacterSize(48);
    timeText.setFillColor(sf::Color::White);
    timeText.setPosition(250, 70);

    // Stopwatch Label
    sf::Text stopwatchLabel;
    stopwatchLabel.setFont(font);
    stopwatchLabel.setCharacterSize(20);
    stopwatchLabel.setFillColor(sf::Color::Cyan);
    stopwatchLabel.setPosition(50, 150);
    stopwatchLabel.setString("Stopwatch:");

    // Stopwatch Display
    sf::Text stopwatchText;
    stopwatchText.setFont(font);
    stopwatchText.setCharacterSize(36);
    stopwatchText.setFillColor(sf::Color::Green);
    stopwatchText.setPosition(250, 145);

    // Timer Label
    sf::Text timerLabel;
    timerLabel.setFont(font);
    timerLabel.setCharacterSize(20);
    timerLabel.setFillColor(sf::Color::Cyan);
    timerLabel.setPosition(50, 210);
    timerLabel.setString("Timer:");

    // Timer Display
    sf::Text timerText;
    timerText.setFont(font);
    timerText.setCharacterSize(36);
    timerText.setFillColor(sf::Color::Magenta);
    timerText.setPosition(250, 205);

    // Alarm Status
    sf::Text alarmText;
    alarmText.setFont(font);
    alarmText.setCharacterSize(28);
    alarmText.setFillColor(sf::Color::Green);
    alarmText.setPosition(50, 270);
    alarmText.setString("Alarm: No Alarm Set");

    // Menu Instructions
    sf::Text menuText;
    menuText.setFont(font);
    menuText.setCharacterSize(18);
    menuText.setFillColor(sf::Color::White);
    menuText.setPosition(50, 330);
    menuText.setString(
        "==================== INSTRUCTION MANUAL ====================\n\n"
        "STOPWATCH CONTROLS:\n"
        "  S - Start/Resume Stopwatch\n"
        "  P - Pause Stopwatch (freezes the time)\n"
        "  R - Reset Stopwatch (back to 00:00:00)\n\n"
        "TIMER CONTROLS:\n"
        "  T - Quick Start (30 seconds countdown)\n"
        "  1 - Set Timer for 1 minute\n"
        "  2 - Set Timer for 2 minutes\n"
        "  3 - Set Timer for 5 minutes\n"
        "  O - Stop/Cancel Timer\n"
        "  Note: Timer counts down to zero, then displays 'TIMER DONE!'\n\n"
        "ALARM CONTROLS:\n"
        "  A - Set Alarm (triggers 10 seconds from now)\n\n"
        "SYSTEM:\n"
        "  ESC - Exit Application\n"
        "============================================================"
    );

    // 6. MAIN EVENT LOOP
    char buffer;
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
                isRunning = false;
            }
            
            // MENU-BASED NAVIGATION
            if (event.type == sf::Event::KeyPressed) {
                switch (event.key.code) {
                    // Stopwatch Controls
                    case sf::Keyboard::S:
                        {
                            lock_guard<mutex> lock(controlMutex);
                            if (!stopwatchRunning) {
                                stopwatchRunning = true;
                                stopwatchPaused = false;
                            } else if (stopwatchPaused) {
                                stopwatchPaused = false;
                            }
                        }
                        break;
                    
                    case sf::Keyboard::P:
                        {
                            lock_guard<mutex> lock(controlMutex);
                            if (stopwatchRunning && !stopwatchPaused) {
                                stopwatchPaused = true;
                            }
                        }
                        break;
                    
                    case sf::Keyboard::R:
                        {
                            lock_guard<mutex> lock1(controlMutex);
                            lock_guard<mutex> lock2(stopwatchMutex);
                            stopwatchRunning = false;
                            stopwatchPaused = false;
                            stopwatchHours = 0;
                            stopwatchMinutes = 0;
                            stopwatchSeconds = 0;
                            stopwatchStr = "00:00:00";
                        }
                        break;
                    
                    // Timer Controls
                    case sf::Keyboard::T:
                        {
                            lock_guard<mutex> lock1(controlMutex);
                            lock_guard<mutex> lock2(timerMutex);
                            timerMinutes = 0;
                            timerSeconds = 30;
                            timerRunning = true;
                            timerPaused = false;
                        }
                        break;
                    
                    case sf::Keyboard::Num1:
                        {
                            lock_guard<mutex> lock1(controlMutex);
                            lock_guard<mutex> lock2(timerMutex);
                            timerMinutes = 1;
                            timerSeconds = 0;
                            timerRunning = true;
                            timerPaused = false;
                        }
                        break;
                    
                    case sf::Keyboard::Num2:
                        {
                            lock_guard<mutex> lock1(controlMutex);
                            lock_guard<mutex> lock2(timerMutex);
                            timerMinutes = 2;
                            timerSeconds = 0;
                            timerRunning = true;
                            timerPaused = false;
                        }
                        break;
                    
                    case sf::Keyboard::Num3:
                        {
                            lock_guard<mutex> lock1(controlMutex);
                            lock_guard<mutex> lock2(timerMutex);
                            timerMinutes = 5;
                            timerSeconds = 0;
                            timerRunning = true;
                            timerPaused = false;
                        }
                        break;
                    
                    case sf::Keyboard::O:
                        {
                            lock_guard<mutex> lock1(controlMutex);
                            lock_guard<mutex> lock2(timerMutex);
                            timerRunning = false;
                            timerPaused = false;
                            timerStr = "00:00";
                        }
                        break;
                    
                    // Alarm Control
                    case sf::Keyboard::A:
                        {
                            // Set alarm for 10 seconds from now
                            time_t t = time(nullptr) + 10;
                            tm* future = localtime(&t);
                            
                            stringstream ss;
                            ss << setfill('0') << setw(2) << future->tm_hour << ":"
                               << setfill('0') << setw(2) << future->tm_min << ":"
                               << setfill('0') << setw(2) << future->tm_sec;
                            
                            string alarmTime = ss.str();
                            
                            {
                                lock_guard<mutex> lock(alarmMutex);
                                alarmStatus = "Alarm set for: " + alarmTime;
                                alarmRinging = false;
                            }
                            
                            // Send to child process via pipe (outside of locks)
                            ssize_t written = write(pipeParentToChild[1], alarmTime.c_str(), alarmTime.length());
                            if (written < 0) {
                                cerr << "Error writing to pipe" << endl;
                            }
                        }
                        break;
                    
                    case sf::Keyboard::Escape:
                        {
                            lock_guard<mutex> lock(controlMutex);
                            window.close();
                            isRunning = false;
                        }
                        break;
                    
                    default:
                        break;
                }
            }
        }

        // --- IPC CHECK: Check Pipe for Alarm Signal ---
        if (read(pipeChildToParent[0], &buffer, 1) > 0) {
            if (buffer == 'A') {
                lock_guard<mutex> lock(alarmMutex);
                alarmStatus = "*** ALARM RINGING!!! ***";
                alarmRinging = true;
            }
        }

        // --- THREAD SYNC: Read Shared Data ---
        {
            lock_guard<mutex> lock(displayMutex);
            timeText.setString(sharedTimeStr);
        }
        
        {
            lock_guard<mutex> lock(stopwatchMutex);
            stopwatchText.setString(stopwatchStr);
        }
        
        {
            lock_guard<mutex> lock(timerMutex);
            timerText.setString(timerStr);
        }
        
        {
            lock_guard<mutex> lock(alarmMutex);
            alarmText.setString("Alarm: " + alarmStatus);
            if (alarmRinging) {
                alarmText.setFillColor(sf::Color::Red);
            } else {
                alarmText.setFillColor(sf::Color::Green);
            }
        }

        // --- RENDER ---
        window.clear(sf::Color(20, 20, 40));
        window.draw(titleText);
        window.draw(clockLabel);
        window.draw(timeText);
        window.draw(stopwatchLabel);
        window.draw(stopwatchText);
        window.draw(timerLabel);
        window.draw(timerText);
        window.draw(alarmText);
        window.draw(menuText);
        window.display();
    }

    // 7. CLEANUP
    if (bgClock.joinable()) bgClock.join();
    if (bgStopwatch.joinable()) bgStopwatch.join();
    if (bgTimer.joinable()) bgTimer.join();
    
    close(pipeParentToChild[1]);
    close(pipeChildToParent[0]);
    
    // Kill child process
    kill(pid, SIGKILL);
    
    // Destroy semaphore
    sem_destroy(&alarmSemaphore);

    return 0;
}
