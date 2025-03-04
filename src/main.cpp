/*
 * Pomi - A GBA Pomodoro Timer
 * A productivity timer for Game Boy Advance
 * Implemented using Butano engine
 */
#include "bn_core.h"
#include "bn_timer.h"
#include "bn_keypad.h"
#include "bn_string.h"
#include "bn_string_view.h"
#include "bn_timers.h"
#include "bn_bg_palettes.h"
#include "bn_sprite_text_generator.h"

#include "common_info.h"
#include "common_variable_8x16_sprite_font.h"

// Pomodoro States
enum class PomodoroState {
    IDLE,
    WORK,
    SHORT_BREAK,
    LONG_BREAK,
    CONFIG
};

// Pomodoro Configuration
struct PomodoroConfig {
    int workTime = 25 * 60;         // 25 minutes
    int shortBreakTime = 5 * 60;    // 5 minutes
    int longBreakTime = 15 * 60;    // 15 minutes
    int sessionsPerSet = 4;         // 4 sessions before a long break
    bn::color workColor = bn::color(31, 0, 0);     // Red
    bn::color shortColor = bn::color(0, 31, 0);    // Green
    bn::color longColor = bn::color(0, 0, 31);     // Blue
};

// Pomodoro Context
struct PomodoroContext {
    PomodoroConfig config;
    PomodoroState state = PomodoroState::IDLE;
    int secondsRemaining = 0;
    int completedSessions = 0;
    int completedSets = 0;
    bool timerActive = false;
    int configSelection = 0;
    bn::timer timer;
    long long lastTicks = 0;  // Use long long instead of uint64_t
};

// Screen dimensions
constexpr int SCREEN_WIDTH = 240;
constexpr int SCREEN_HEIGHT = 160;
constexpr int SCREEN_CENTER_X = SCREEN_WIDTH / 2;
constexpr int SCREEN_CENTER_Y = SCREEN_HEIGHT / 2;

// Function declarations
void changeState(PomodoroContext& ctx, PomodoroState newState);
void drawProgressBar(bn::sprite_text_generator& text_generator, bn::vector<bn::sprite_ptr, 128>& sprites, 
                   int current, int total, bn::color color);
void handleInput(PomodoroContext& ctx);
void updateTimer(PomodoroContext& ctx);
void renderPomodoro(PomodoroContext& ctx, bn::sprite_text_generator& text_generator, 
                  bn::vector<bn::sprite_ptr, 128>& sprites);
void renderTimer(PomodoroContext& ctx, bn::sprite_text_generator& text_generator, 
                bn::vector<bn::sprite_ptr, 128>& sprites);
void renderConfig(PomodoroContext& ctx, bn::sprite_text_generator& text_generator, 
                bn::vector<bn::sprite_ptr, 128>& sprites);
void playSound(int frequency, int duration);
void renderTimerText(bn::sprite_text_generator& text_generator, bn::vector<bn::sprite_ptr, 128>& sprites, long long seconds);
void drawHorizontalLine(bn::sprite_text_generator& text_generator, bn::vector<bn::sprite_ptr, 128>& sprites,
                      int y, int width, bn::color color);
void drawVerticalLine(bn::sprite_text_generator& text_generator, bn::vector<bn::sprite_ptr, 128>& sprites,
                     int x, int y1, int y2, bn::color color);
void drawPanel(bn::sprite_text_generator& text_generator, bn::vector<bn::sprite_ptr, 128>& sprites,
              int x, int y, int width, int height, bn::color color, const bn::string_view& title);

int main()
{
    // Initialize the Butano engine
    bn::core::init();
    
    // Game setup
    bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
    bn::vector<bn::sprite_ptr, 128> text_sprites;
    text_generator.set_center_alignment();
    
    // Set background color to dark blue for space-like feel
    bn::bg_palettes::set_transparent_color(bn::color(0, 0, 8)); // Very dark blue
    
    // Initialize Pomodoro context
    PomodoroContext ctx;
    ctx.secondsRemaining = ctx.config.workTime;
    ctx.state = PomodoroState::WORK;  // Set initial state to WORK instead of default IDLE
    
    // Main game loop
    while(true)
    {
        // Handle user input
        handleInput(ctx);
        
        // Update timer
        updateTimer(ctx);
        
        // Clear previous text sprites and create new empty vector for this frame
        text_sprites = bn::vector<bn::sprite_ptr, 128>();
        
        // Render appropriate screen based on current state
        if(ctx.state == PomodoroState::CONFIG) {
            renderConfig(ctx, text_generator, text_sprites);
        } else {
            renderPomodoro(ctx, text_generator, text_sprites);
        }
        
        // Process frame and wait for next
        bn::core::update();
    }
}

// Update the timer state
void updateTimer(PomodoroContext& ctx) {
    // Only update if timer is active
    if (!ctx.timerActive) {
        return;
    }
    
    // Get elapsed time
    long long currentTicks = ctx.timer.elapsed_ticks();
    long long elapsedTicks = currentTicks - ctx.lastTicks;
    
    // Convert to seconds using Butano's utility
    if (elapsedTicks >= bn::timers::ticks_per_second()) {
        // Calculate how many seconds have elapsed
        int elapsedSeconds = elapsedTicks / bn::timers::ticks_per_second();
        
        // Decrease the remaining time
        ctx.secondsRemaining -= elapsedSeconds;
        
        // Update the last tick count
        ctx.lastTicks = currentTicks;
        
        // Check if timer has ended
        if (ctx.secondsRemaining <= 0) {
            // Timer finished
            ctx.timerActive = false;
            ctx.secondsRemaining = 0;
            
            // Play sound to alert user
            playSound(440, 30); // A4 note
            
            // Switch to next state
            // If we're in a work session, track completion
            if (ctx.state == PomodoroState::WORK) {
                ctx.completedSessions++;
                
                // Check if we've completed a full set
                if (ctx.completedSessions % ctx.config.sessionsPerSet == 0) {
                    ctx.completedSets++;
                    changeState(ctx, PomodoroState::LONG_BREAK);
                } else {
                    changeState(ctx, PomodoroState::SHORT_BREAK);
                }
            } else if (ctx.state == PomodoroState::SHORT_BREAK || 
                     ctx.state == PomodoroState::LONG_BREAK) {
                // After a break, go back to work
                changeState(ctx, PomodoroState::WORK);
            }
        }
    }
}

// Handle user input
void handleInput(PomodoroContext& ctx) {
    if (ctx.state == PomodoroState::CONFIG) {
        // Configuration mode input handling
        
        // Navigate configuration options
        if (bn::keypad::up_pressed()) {
            if (ctx.configSelection > 0) {
                ctx.configSelection--;
            } else {
                ctx.configSelection = 3; // Wrap to last option (we now have only 4 options)
            }
        }
        
        if (bn::keypad::down_pressed()) {
            if (ctx.configSelection < 3) { // Now only 4 options (0-3)
                ctx.configSelection++;
            } else {
                ctx.configSelection = 0; // Wrap to first option
            }
        }
        
        // Adjust values
        int change = 0;
        if (bn::keypad::left_pressed()) {
            change = -1;
        }
        if (bn::keypad::right_pressed()) {
            change = 1;
        }
        
        if (change != 0) {
            // Apply change to the selected config item
            switch (ctx.configSelection) {
                case 0: // Work time
                    ctx.config.workTime += change * 60;
                    if (ctx.config.workTime < 60) {
                        ctx.config.workTime = 60;  // Minimum 1 minute
                    }
                    break;
                case 1: // Short break
                    ctx.config.shortBreakTime += change * 60;
                    if (ctx.config.shortBreakTime < 60) {
                        ctx.config.shortBreakTime = 60;  // Minimum 1 minute
                    }
                    break;
                case 2: // Long break
                    ctx.config.longBreakTime += change * 60;
                    if (ctx.config.longBreakTime < 60) {
                        ctx.config.longBreakTime = 60;  // Minimum 1 minute
                    }
                    break;
                case 3: // Sessions per set
                    ctx.config.sessionsPerSet += change;
                    if (ctx.config.sessionsPerSet < 1) {
                        ctx.config.sessionsPerSet = 1;  // Minimum 1 session
                    }
                    break;
            }
        }
        
        // Exit config mode
        if (bn::keypad::b_pressed() || bn::keypad::select_pressed()) {
            changeState(ctx, PomodoroState::IDLE);
        }
    } else {
        // Normal operation mode input handling
        
        // Toggle timer
        if (bn::keypad::a_pressed()) {
            ctx.timerActive = !ctx.timerActive;
            
            // If starting timer, reset tick counter
            if (ctx.timerActive) {
                ctx.lastTicks = ctx.timer.elapsed_ticks();
            }
        }
        
        // Reset timer
        if (bn::keypad::b_pressed()) {
            ctx.timerActive = false;
            // Reset the tick counter
            ctx.lastTicks = ctx.timer.elapsed_ticks();
            
            // Reset to appropriate duration based on current state
            if (ctx.state == PomodoroState::WORK) {
                ctx.secondsRemaining = ctx.config.workTime;
            } else if (ctx.state == PomodoroState::SHORT_BREAK) {
                ctx.secondsRemaining = ctx.config.shortBreakTime;
            } else if (ctx.state == PomodoroState::LONG_BREAK) {
                ctx.secondsRemaining = ctx.config.longBreakTime;
            } else {
                ctx.secondsRemaining = ctx.config.workTime;
            }
        }
        
        // Enter config mode
        if (bn::keypad::select_pressed()) {
            ctx.timerActive = false;
            changeState(ctx, PomodoroState::CONFIG);
        }
    }
}

// Render the Pomodoro timer screen
void renderPomodoro(PomodoroContext& ctx, bn::sprite_text_generator& text_generator,
                  bn::vector<bn::sprite_ptr, 128>& sprites) {
    bn::color stateColor;
    bn::string_view stateText;
    
    // Determine state color and text based on current state and timer activity
    if (ctx.state == PomodoroState::WORK) {
        stateColor = ctx.config.workColor;
        stateText = ctx.timerActive ? "WORK" : "WORK - PAUSED";
    } else if (ctx.state == PomodoroState::SHORT_BREAK) {
        stateColor = ctx.config.shortColor;
        stateText = ctx.timerActive ? "SHORT REST" : "SHORT REST - PAUSED";
    } else if (ctx.state == PomodoroState::LONG_BREAK) {
        stateColor = ctx.config.longColor;
        stateText = ctx.timerActive ? "LONG REST" : "LONG REST - PAUSED";
    } else {
        stateColor = bn::color(31, 31, 31);
        stateText = "STANDBY";
    }
    
    // Set center alignment for all text
    text_generator.set_center_alignment();
    
    // Draw title
    text_generator.generate(0, -70, "POMI", sprites);
    
    // Draw state panel with minimal decorations
    drawPanel(text_generator, sprites, 0, -20, 160, 50, stateColor, "STATUS");
    
    // Draw current state
    text_generator.generate(0, -40, stateText, sprites);
    
    // Render timer display (centered on screen)
    renderTimerText(text_generator, sprites, ctx.secondsRemaining);
    
    // Draw stats with balanced spacing
    text_generator.generate(-5, 20, "CYCLES:", sprites);
    
    // Display session counter
    bn::string<8> sessionText = bn::to_string<4>(ctx.completedSessions);
    text_generator.generate(25, 20, sessionText, sprites);
    
    // Draw controls panel
    drawPanel(text_generator, sprites, 0, 80, 160, 30, bn::color(0, 31, 31), "COMMANDS");
    
    // Create dynamic command text based on timer state
    bn::string<64> commandText;
    if (ctx.timerActive) {
        commandText = "Pause:A Reset:B Config:SELECT";
    } else {
        commandText = "Start:A Reset:B Config:SELECT";
    }
    
    // Show controls with proper spacing
    text_generator.generate(0, 70, commandText, sprites);
}

// Render the configuration menu
void renderConfig(PomodoroContext& ctx, bn::sprite_text_generator& text_generator,
                bn::vector<bn::sprite_ptr, 128>& sprites) {
    // Create new empty vector to replace the current one
    sprites = bn::vector<bn::sprite_ptr, 128>();
    
    // Ensure center alignment
    text_generator.set_center_alignment();
    
    // Draw title
    text_generator.generate(0, -70, "CONFIG", sprites);
    
    // Draw simplified config panel
    drawPanel(text_generator, sprites, 0, 0, 160, 100, bn::color(0, 31, 31), "PARAMS");
    
    // Define simplified config items
    const char* item_labels[] = {
        "WORK:", "S.REST:", "L.REST:", "SET SIZE:"
    };
    
    // Display only 4 key config items
    for (int i = 0; i < 4; ++i) {
        bn::string<24> itemText = item_labels[i];
        
        // Show selection indicator
        if (ctx.configSelection == i) {
            text_generator.generate(-75, -40 + i * 20, ">", sprites);
        }
        
        // Add value for each item
        switch (i) {
            case 0: // Work time
                itemText.append(bn::to_string<4>(ctx.config.workTime / 60));
                itemText.append("m");
                break;
            case 1: // Short break
                itemText.append(bn::to_string<4>(ctx.config.shortBreakTime / 60));
                itemText.append("m");
                break;
            case 2: // Long break
                itemText.append(bn::to_string<4>(ctx.config.longBreakTime / 60));
                itemText.append("m");
                break;
            case 3: // Sessions per set
                itemText.append(bn::to_string<4>(ctx.config.sessionsPerSet));
                break;
        }
        
        // Display the item
        text_generator.generate(0, -40 + i * 20, itemText, sprites);
    }
    
    // Draw controls at bottom
    text_generator.generate(0, 60, "NAVIGATE:\x18\x19 ADJUST:\x1A\x1B EXIT:B", sprites);
}

// Draw a progress bar
void drawProgressBar(bn::sprite_text_generator& text_generator, bn::vector<bn::sprite_ptr, 128>& sprites,
                   int current, int total, bn::color color) {
    // Calculate progress (0-100%)
    int progress = total > 0 ? (current * 100) / total : 0;
    
    // Create a string_view for the blocks we'll use
    bn::string_view full_block = "■";
    bn::string_view empty_block = "□";
    
    // Draw the progress bar as a series of characters - use a larger buffer
    bn::string<128> barText = "[";
    
    // Use different characters for a more futuristic look
    for (int i = 0; i < 20; ++i) {
        if (i * 5 < progress) {
            barText.append(full_block);
        } else {
            barText.append(empty_block);
        }
    }
    barText.append("]");
    
    // Draw the bar
    text_generator.generate(0, 15, barText, sprites);
    
    // Draw percentage
    bn::string<16> percentText = bn::to_string<8>(progress);
    percentText.append("%");
    text_generator.generate(0, 30, percentText, sprites);
}

// Change the timer state
void changeState(PomodoroContext& ctx, PomodoroState newState) {
    // Store previous state for transition effects
    PomodoroState oldState = ctx.state;
    
    // Update state
    ctx.state = newState;
    
    // Reset timer activity when changing states
    if (oldState != newState && newState != PomodoroState::CONFIG) {
        ctx.timerActive = false;
    }
    
    // Set the new timer based on the state
    switch (newState) {
        case PomodoroState::WORK:
            ctx.secondsRemaining = ctx.config.workTime;
            break;
        case PomodoroState::SHORT_BREAK:
            ctx.secondsRemaining = ctx.config.shortBreakTime;
            break;
        case PomodoroState::LONG_BREAK:
            ctx.secondsRemaining = ctx.config.longBreakTime;
            break;
        default:
            // For IDLE or CONFIG, use the work time as default
            ctx.secondsRemaining = ctx.config.workTime;
            break;
    }
    
    // Play transition sound
    if (oldState != newState) {
        // Different sounds for different transitions
        if (newState == PomodoroState::WORK) {
            playSound(1500, 20);
        } else if (newState == PomodoroState::SHORT_BREAK || newState == PomodoroState::LONG_BREAK) {
            playSound(800, 20);
        } else if (newState == PomodoroState::IDLE) {
            playSound(500, 10);
        }
    }
}

// Play a sound effect
void playSound(int frequency, int duration) {
    // Simple placeholder function
    // In a real implementation, this would use Butano's audio features
    (void)frequency;
    (void)duration;
}

// Draw a horizontal line
void drawHorizontalLine(bn::sprite_text_generator& text_generator, bn::vector<bn::sprite_ptr, 128>& sprites,
                      int y, int width, bn::color color) {
    int halfWidth = width / 2;
    
    // Create string for horizontal line
    bn::string<128> line;
    
    // Fill with horizontal line characters - make sure we don't overflow
    int maxChars = halfWidth > 126 ? 126 : halfWidth; // Leave room for null terminator
    for (int i = 0; i < maxChars; ++i) {
        line += "-";
    }
    
    // Center alignment for line
    text_generator.set_center_alignment();
    text_generator.generate(0, y, line, sprites);
    text_generator.set_left_alignment();
}

// Draw a vertical line (simplified)
void drawVerticalLine(bn::sprite_text_generator& text_generator, bn::vector<bn::sprite_ptr, 128>& sprites,
                     int x, int y1, int y2, bn::color color) {
    // Draw vertical line by stacking characters
    for (int y = y1; y <= y2; y += 8) {
        text_generator.generate(x, y, "|", sprites);
    }
}

// Draw a simplified panel with title
void drawPanel(bn::sprite_text_generator& text_generator, bn::vector<bn::sprite_ptr, 128>& sprites,
              int x, int y, int width, int height, bn::color color, const bn::string_view& title) {
    // Just draw the title at the top of where the panel would be
    if (!title.empty()) {
        // Calculate the top border position
        int topBorderY = y - height/2;
        
        // Draw a simple header
        bn::string<32> header = "[ ";
        header.append(title);
        header.append(" ]");
        
        // Generate title at the top center of the panel using the provided x coordinate
        text_generator.generate(x, topBorderY - 8, header, sprites);
    }
}

// Render timer text as MM:SS
void renderTimerText(bn::sprite_text_generator& text_generator, bn::vector<bn::sprite_ptr, 128>& sprites, long long seconds) {
    // Convert seconds to minutes and remaining seconds
    int minutes = seconds / 60;
    int remainingSecs = seconds % 60;
    
    // Format time as MM:SS
    bn::string<8> timerText;
    
    // Add minutes
    if (minutes < 10) {
        timerText.append("0");
    }
    timerText.append(bn::to_string<2>(minutes));
    
    timerText.append(":");
    
    // Add seconds
    if (remainingSecs < 10) {
        timerText.append("0");
    }
    timerText.append(bn::to_string<2>(remainingSecs));
    
    // Generate the sprite centered on screen
    text_generator.generate(0, 0, timerText, sprites);
}

// Render the timer screen
void renderTimer(PomodoroContext& ctx, bn::sprite_text_generator& text_generator, 
                bn::vector<bn::sprite_ptr, 128>& sprites) {
    // Use string_view for static UI text
    bn::string_view title_text = "MISSION TIMER";
    bn::string_view time_panel = "TIME";
    bn::string_view progress_panel = "PROGRESS";
    bn::string_view command_panel = "COMMAND";
    
    // Dynamically choose start/pause text based on timer state
    bn::string_view start_text = ctx.timerActive ? "PAUSE: A" : "START: A";
    bn::string_view reset_text = "RESET: B";
    bn::string_view menu_text = "MENU: SELECT";
    
    // Create new empty vector to replace the current one
    sprites = bn::vector<bn::sprite_ptr, 128>();
    
    // Draw title
    text_generator.generate(0, -70, title_text, sprites);
    
    // Draw timer panel
    drawPanel(text_generator, sprites, 0, -30, 100, 50, bn::color(0, 31, 31), time_panel);
    
    // Draw timer
    renderTimerText(text_generator, sprites, ctx.secondsRemaining);
    
    // Draw progress panel
    drawPanel(text_generator, sprites, 0, 25, 200, 40, bn::color(0, 31, 31), progress_panel);
    
    // Calculate total time based on current state
    int totalTime = 0;
    if (ctx.state == PomodoroState::WORK) {
        totalTime = ctx.config.workTime;
    } else if (ctx.state == PomodoroState::SHORT_BREAK) {
        totalTime = ctx.config.shortBreakTime;
    } else if (ctx.state == PomodoroState::LONG_BREAK) {
        totalTime = ctx.config.longBreakTime;
    } else {
        totalTime = 1; // Prevent division by zero
    }
    
    // Calculate elapsed time (total - remaining)
    int elapsed = totalTime - ctx.secondsRemaining;
    
    // Draw progress bar with color based on state
    bn::color progressColor = bn::color(31, 31, 31); // Default white
    if (ctx.state == PomodoroState::WORK) {
        progressColor = ctx.config.workColor;
    } else if (ctx.state == PomodoroState::SHORT_BREAK) {
        progressColor = ctx.config.shortColor;
    } else if (ctx.state == PomodoroState::LONG_BREAK) {
        progressColor = ctx.config.longColor;
    }
    
    // Draw progress bar with correct parameters
    drawProgressBar(text_generator, sprites, elapsed, totalTime, progressColor);
    
    // Draw command panel
    drawPanel(text_generator, sprites, 0, 75, 200, 30, bn::color(0, 31, 31), command_panel);
    
    // Draw controls
    text_generator.generate(-60, 75, start_text, sprites);
    text_generator.generate(0, 75, reset_text, sprites);
    text_generator.generate(60, 75, menu_text, sprites);
}