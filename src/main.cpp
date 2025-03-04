/*
 * GBA Pomodoro Timer
 * A productivity timer for Game Boy Advance
 * Implemented using Butano engine
 */

#include "bn_core.h"
#include "bn_timer.h"
#include "bn_keypad.h"
#include "bn_string.h"
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
    uint64_t lastTicks = 0;
};

// Screen dimensions
constexpr int SCREEN_WIDTH = 240;
constexpr int SCREEN_HEIGHT = 160;
constexpr int SCREEN_CENTER_X = SCREEN_WIDTH / 2;
constexpr int SCREEN_CENTER_Y = SCREEN_HEIGHT / 2;

// Function declarations
void changeState(PomodoroContext& ctx, PomodoroState newState);
void drawProgressBar(bn::sprite_text_generator& text_generator, bn::vector<bn::sprite_ptr, 64>& sprites, 
                    int elapsed, int total, bn::color color);
void handleInput(PomodoroContext& ctx);
void updateTimer(PomodoroContext& ctx);
void renderPomodoro(PomodoroContext& ctx, bn::sprite_text_generator& text_generator, 
                   bn::vector<bn::sprite_ptr, 64>& sprites);
void renderConfig(PomodoroContext& ctx, bn::sprite_text_generator& text_generator, 
                 bn::vector<bn::sprite_ptr, 64>& sprites);
void playSound(int frequency, int duration);

int main()
{
    bn::core::init();
    
    // Initialize text generator
    bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
    bn::vector<bn::sprite_ptr, 64> text_sprites;
    text_generator.set_center_alignment();
    
    // Set background color
    bn::bg_palettes::set_transparent_color(bn::color(0, 0, 0));
    
    // Initialize Pomodoro context
    PomodoroContext ctx;
    ctx.secondsRemaining = ctx.config.workTime;
    
    while(true)
    {
        // Handle user input
        handleInput(ctx);
        
        // Update timer
        updateTimer(ctx);
        
        // Clear previous text sprites
        text_sprites.clear();
        
        // Render appropriate screen
        if(ctx.state == PomodoroState::CONFIG) {
            renderConfig(ctx, text_generator, text_sprites);
        } else {
            renderPomodoro(ctx, text_generator, text_sprites);
        }
        
        bn::core::update();
    }
}

// Update the timer state
void updateTimer(PomodoroContext& ctx) {
    if (ctx.state == PomodoroState::CONFIG || !ctx.timerActive) {
        // Reset timer when not active
        ctx.lastTicks = ctx.timer.elapsed_ticks();
        return;
    }
    
    // Get elapsed time
    uint64_t currentTicks = ctx.timer.elapsed_ticks();
    uint64_t elapsedTicks = currentTicks - ctx.lastTicks;
    
    // Convert to seconds using Butano's utility
    if (elapsedTicks >= bn::timers::ticks_per_second()) {
        // Restart the timer accounting for any remainder
        ctx.lastTicks = currentTicks;
        
        // Decrement the timer
        if (ctx.secondsRemaining > 0) {
            ctx.secondsRemaining--;
            
            // Play a tick sound at each minute
            if (ctx.secondsRemaining % 60 == 0) {
                playSound(1000, 5);
            }
        } else {
            // Timer completed, handle state transition
            playSound(2000, 30);
            
            switch (ctx.state) {
                case PomodoroState::WORK:
                    ctx.completedSessions++;
                    
                    // Every N sessions, take a long break
                    if (ctx.completedSessions % ctx.config.sessionsPerSet == 0) {
                        ctx.completedSets++;
                        changeState(ctx, PomodoroState::LONG_BREAK);
                    } else {
                        changeState(ctx, PomodoroState::SHORT_BREAK);
                    }
                    break;
                    
                case PomodoroState::SHORT_BREAK:
                case PomodoroState::LONG_BREAK:
                    changeState(ctx, PomodoroState::WORK);
                    break;
                    
                default:
                    break;
            }
        }
    }
}

// Handle user input
void handleInput(PomodoroContext& ctx) {
    if (ctx.state == PomodoroState::CONFIG) {
        // Config mode input handling
        if (bn::keypad::up_pressed()) {
            ctx.configSelection = (ctx.configSelection + 6) % 7;
        } else if (bn::keypad::down_pressed()) {
            ctx.configSelection = (ctx.configSelection + 1) % 7;
        }
        
        if (bn::keypad::left_pressed() || bn::keypad::right_pressed()) {
            int change = bn::keypad::left_pressed() ? -1 : 1;
            
            switch (ctx.configSelection) {
                case 0: // Work time
                    ctx.config.workTime = bn::max(1, ctx.config.workTime + change * 60); 
                    break;
                case 1: // Short break
                    ctx.config.shortBreakTime = bn::max(1, ctx.config.shortBreakTime + change * 60);
                    break;
                case 2: // Long break
                    ctx.config.longBreakTime = bn::max(1, ctx.config.longBreakTime + change * 60);
                    break;
                case 3: // Sessions per set
                    ctx.config.sessionsPerSet = bn::max(1, ctx.config.sessionsPerSet + change);
                    break;
                case 4: // Work color
                    // Cycle through predefined colors - simplified for example
                    if (change > 0) {
                        if (ctx.config.workColor == bn::color(31, 0, 0)) ctx.config.workColor = bn::color(0, 31, 0);
                        else if (ctx.config.workColor == bn::color(0, 31, 0)) ctx.config.workColor = bn::color(0, 0, 31);
                        else ctx.config.workColor = bn::color(31, 0, 0);
                    } else {
                        if (ctx.config.workColor == bn::color(0, 0, 31)) ctx.config.workColor = bn::color(0, 31, 0);
                        else if (ctx.config.workColor == bn::color(0, 31, 0)) ctx.config.workColor = bn::color(31, 0, 0);
                        else ctx.config.workColor = bn::color(0, 0, 31);
                    }
                    break;
                case 5: // Short break color
                    if (change > 0) {
                        if (ctx.config.shortColor == bn::color(31, 0, 0)) ctx.config.shortColor = bn::color(0, 31, 0);
                        else if (ctx.config.shortColor == bn::color(0, 31, 0)) ctx.config.shortColor = bn::color(0, 0, 31);
                        else ctx.config.shortColor = bn::color(31, 0, 0);
                    } else {
                        if (ctx.config.shortColor == bn::color(0, 0, 31)) ctx.config.shortColor = bn::color(0, 31, 0);
                        else if (ctx.config.shortColor == bn::color(0, 31, 0)) ctx.config.shortColor = bn::color(31, 0, 0);
                        else ctx.config.shortColor = bn::color(0, 0, 31);
                    }
                    break;
                case 6: // Long break color
                    if (change > 0) {
                        if (ctx.config.longColor == bn::color(31, 0, 0)) ctx.config.longColor = bn::color(0, 31, 0);
                        else if (ctx.config.longColor == bn::color(0, 31, 0)) ctx.config.longColor = bn::color(0, 0, 31);
                        else ctx.config.longColor = bn::color(31, 0, 0);
                    } else {
                        if (ctx.config.longColor == bn::color(0, 0, 31)) ctx.config.longColor = bn::color(0, 31, 0);
                        else if (ctx.config.longColor == bn::color(0, 31, 0)) ctx.config.longColor = bn::color(31, 0, 0);
                        else ctx.config.longColor = bn::color(0, 0, 31);
                    }
                    break;
                default:
                    break;
            }
        }
        
        // Exit config
        if (bn::keypad::b_pressed() || bn::keypad::select_pressed()) {
            ctx.state = PomodoroState::IDLE;
            ctx.secondsRemaining = ctx.config.workTime;
        }
    } else {
        // Normal mode input handling
        if (bn::keypad::a_pressed()) {
            // Toggle timer
            ctx.timerActive = !ctx.timerActive;
            
            // If just started and in IDLE state, change to WORK
            if (ctx.timerActive && ctx.state == PomodoroState::IDLE) {
                changeState(ctx, PomodoroState::WORK);
            }
        }
        
        if (bn::keypad::b_pressed()) {
            // Reset timer
            ctx.timerActive = false;
            ctx.state = PomodoroState::IDLE;
            ctx.secondsRemaining = ctx.config.workTime;
            ctx.completedSessions = 0;
            ctx.completedSets = 0;
        }
        
        if (bn::keypad::start_pressed()) {
            // Skip to next state
            ctx.secondsRemaining = 0;
            updateTimer(ctx);
        }
        
        if (bn::keypad::select_pressed()) {
            // Enter config mode
            ctx.timerActive = false;
            ctx.state = PomodoroState::CONFIG;
        }
    }
}

// Render the Pomodoro timer screen
void renderPomodoro(PomodoroContext& ctx, bn::sprite_text_generator& text_generator,
                  bn::vector<bn::sprite_ptr, 64>& sprites) {
    bn::color stateColor;
    
    // Choose color based on state
    if (ctx.state == PomodoroState::WORK) {
        stateColor = ctx.config.workColor;
    } else if (ctx.state == PomodoroState::SHORT_BREAK) {
        stateColor = ctx.config.shortColor;
    } else if (ctx.state == PomodoroState::LONG_BREAK) {
        stateColor = ctx.config.longColor;
    } else {
        stateColor = bn::color(31, 31, 31); // White
    }
    
    // Draw title
    text_generator.generate(0, -60, "POMODORO TIMER", sprites);
    
    // Draw state
    bn::string<32> stateText;
    if (ctx.state == PomodoroState::IDLE) {
        stateText = "READY";
    } else if (ctx.state == PomodoroState::WORK) {
        stateText = "WORK";
    } else if (ctx.state == PomodoroState::SHORT_BREAK) {
        stateText = "SHORT BREAK";
    } else if (ctx.state == PomodoroState::LONG_BREAK) {
        stateText = "LONG BREAK";
    }
    text_generator.generate(0, -40, stateText, sprites);
    
    // Draw timer
    int minutes = ctx.secondsRemaining / 60;
    int seconds = ctx.secondsRemaining % 60;
    
    // Format minutes and seconds as strings
    bn::string<32> timerText;
    if (minutes < 10) {
        timerText += "0";
    }
    timerText += bn::to_string<8>(minutes);
    timerText += ":";
    if (seconds < 10) {
        timerText += "0";
    }
    timerText += bn::to_string<8>(seconds);
    
    text_generator.generate(0, -20, timerText, sprites);
    
    // Draw progress bar
    int totalTime;
    if (ctx.state == PomodoroState::WORK) {
        totalTime = ctx.config.workTime;
    } else if (ctx.state == PomodoroState::SHORT_BREAK) {
        totalTime = ctx.config.shortBreakTime;
    } else if (ctx.state == PomodoroState::LONG_BREAK) {
        totalTime = ctx.config.longBreakTime;
    } else {
        totalTime = ctx.config.workTime;
    }
    
    drawProgressBar(text_generator, sprites, totalTime - ctx.secondsRemaining, totalTime, stateColor);
    
    // Draw session info
    bn::string<32> sessionText = "Sessions: ";
    sessionText += bn::to_string<8>(ctx.completedSessions % ctx.config.sessionsPerSet);
    sessionText += "/";
    sessionText += bn::to_string<8>(ctx.config.sessionsPerSet);
    sessionText += " Sets: ";
    sessionText += bn::to_string<8>(ctx.completedSets);
    
    text_generator.generate(0, 20, sessionText, sprites);
    
    // Draw controls
    text_generator.generate(0, 50, "A: START/PAUSE  B: RESET", sprites);
    text_generator.generate(0, 65, "SELECT: SETTINGS  START: SKIP", sprites);
}

// Render the configuration menu
void renderConfig(PomodoroContext& ctx, bn::sprite_text_generator& text_generator,
                bn::vector<bn::sprite_ptr, 64>& sprites) {
    // Draw title
    text_generator.generate(0, -70, "SETTINGS", sprites);
    
    // Define items with their values
    struct ConfigItem {
        bn::string<32> text;
        bool selected;
    };
    
    ConfigItem items[7];
    
    // Work time
    items[0].text = "WORK TIME: ";
    items[0].text += bn::to_string<8>(ctx.config.workTime / 60);
    items[0].text += "m";
    items[0].selected = (ctx.configSelection == 0);
    
    // Short break
    items[1].text = "SHORT BREAK: ";
    items[1].text += bn::to_string<8>(ctx.config.shortBreakTime / 60);
    items[1].text += "m";
    items[1].selected = (ctx.configSelection == 1);
    
    // Long break
    items[2].text = "LONG BREAK: ";
    items[2].text += bn::to_string<8>(ctx.config.longBreakTime / 60);
    items[2].text += "m";
    items[2].selected = (ctx.configSelection == 2);
    
    // Sessions per set
    items[3].text = "SESSIONS PER SET: ";
    items[3].text += bn::to_string<8>(ctx.config.sessionsPerSet);
    items[3].selected = (ctx.configSelection == 3);
    
    // Colors
    items[4].text = "WORK COLOR";
    items[4].selected = (ctx.configSelection == 4);
    
    items[5].text = "SHORT BREAK COLOR";
    items[5].selected = (ctx.configSelection == 5);
    
    items[6].text = "LONG BREAK COLOR";
    items[6].selected = (ctx.configSelection == 6);
    
    // Draw menu items
    for (int i = 0; i < 7; ++i) {
        bn::color itemColor = items[i].selected ? bn::color(0, 31, 31) : bn::color(31, 31, 31);
        text_generator.generate(0, -50 + i * 15, items[i].text, sprites);
        
        // Draw color boxes for color settings
        if (i == 4) {
            // Draw colored rectangle for work color
            text_generator.generate(70, -50 + i * 15, "[    ]", sprites);
        } else if (i == 5) {
            // Draw colored rectangle for short break color
            text_generator.generate(70, -50 + i * 15, "[    ]", sprites);
        } else if (i == 6) {
            // Draw colored rectangle for long break color
            text_generator.generate(70, -50 + i * 15, "[    ]", sprites);
        }
    }
    
    // Draw controls guide
    text_generator.generate(0, 60, "UP/DOWN: NAVIGATE", sprites);
    text_generator.generate(0, 75, "LEFT/RIGHT: CHANGE VALUE", sprites);
    text_generator.generate(0, 90, "SELECT/B: EXIT SETTINGS", sprites);
}

// Draw a progress bar
void drawProgressBar(bn::sprite_text_generator& text_generator, bn::vector<bn::sprite_ptr, 64>& sprites,
                   int elapsed, int total, bn::color color) {
    // Calculate progress (0-100%)
    int progress = (elapsed * 100) / total;
    
    // Draw the progress bar as a series of characters
    bn::string<32> barText;
    barText.append("[");
    for (int i = 0; i < 20; ++i) {
        if (i * 5 < progress) {
            barText.append("=");
        } else {
            barText.append(" ");
        }
    }
    barText.append("]");
    
    // Draw the bar
    text_generator.generate(0, 0, barText, sprites);
}

// Change the timer state
void changeState(PomodoroContext& ctx, PomodoroState newState) {
    ctx.state = newState;
    
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
}

// Play a sound effect
void playSound(int frequency, int duration) {
    // Simple placeholder function
    // In a real implementation, this would use Butano's audio features
    (void)frequency;
    (void)duration;
}