// =====================================================================
// Student Name      : Ariel Fajimiyo
// Student ID Number : C00300811@setu.ie
// Date              : 24/11/2025
// Purpose           : Demonstrates the main raylib basics together in one
//                     program. Includes shapes, input, text, buttons,
//                     movement and a small interactive slider.
// =====================================================================

#include "raylib.h"
#include <cmath>

// Simple reusable button struct
// (Helps keep the main loop cleaner and shows basic UI design)
struct Button {
    Rectangle rect;
    const char* text;

    // Button colours for different states
    Color base = {220,220,220,255};
    Color hover = {180,180,180,255};
    Color click = {100,100,100,255};
    Color textCol = BLACK;

    // Check if mouse is over the button
    bool IsHovered() const { 
        return CheckCollisionPointRec(GetMousePosition(), rect); 
    }

    // True if hovered and clicked
    bool IsClicked() const { 
        return IsHovered() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT); 
    }

    // Draws the button with the right colour depending on interaction
    void Draw() const {
        Color col = IsHovered() ? (IsClicked() ? click : hover) : base;
        DrawRectangleRec(rect, col);
        DrawRectangleLinesEx(rect, 4, DARKGRAY);

        // Center text inside the button
        int w = MeasureText(text, 30);
        DrawText(text, rect.x + (rect.width - w)/2, rect.y + 18, 30, textCol);
    }
};

// Global program state
static bool shapesVisible = true;     // toggled with 1 / mouse click
static float time = 0.0f;             // keeps the bobbing animation moving
static float bobSpeed = 3.0f;         // controlled with the slider

// Slider position under the shapes
const Rectangle sliderRect = { 450, 620, 300, 30 };

int main(void)
{
    const int screenWidth  = 1200;
    const int screenHeight = 800;

    InitWindow(screenWidth, screenHeight, "Ariel Fajimiyo – Part 1");
    SetTargetFPS(60);

    // Simple close button to exit the program
    Button closeBtn{{screenWidth-180, screenHeight-100, 140, 60},
                     "CLOSE", RED, MAROON, RED, WHITE};

    while (!WindowShouldClose())
    {
        // ------------------------------------------------------------
        // INPUT HANDLING
        // ------------------------------------------------------------
        // First check if user is dragging the slider
        bool sliderActive = false;

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            Vector2 mouse = GetMousePosition();

            // If mouse is inside the slider area, adjust bobSpeed
            if (CheckCollisionPointRec(mouse, sliderRect))
            {
                sliderActive = true;     // prevents toggle clicks while sliding

                // Convert mouse X position into a 0 → 1 value
                float ratio = (mouse.x - sliderRect.x) / sliderRect.width;
                if (ratio < 0) ratio = 0;
                if (ratio > 1) ratio = 1;

                // Map ratio to our speed range
                bobSpeed = 0.5f + ratio * 11.5f;
            }
        }

        // Toggle shape visibility (but only if not dragging slider)
        if (!sliderActive &&
            (IsKeyPressed(KEY_ONE) ||
             (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !closeBtn.IsClicked())))
        {
            shapesVisible = !shapesVisible;
        }

        // Exit if the close button gets clicked
        if (closeBtn.IsClicked()) break;

        // ------------------------------------------------------------
        // MOVEMENT
        // ------------------------------------------------------------
        // Simple bobbing animation using a sine wave
        time += GetFrameTime();
        float bob = sinf(time * bobSpeed) * 50.0f;

        // ------------------------------------------------------------
        // DRAWING SECTION
        // ------------------------------------------------------------
        BeginDrawing();
            ClearBackground(RAYWHITE);

            // Program title and subtitle
            DrawText("PART 1 - ALL MODULES",
                     (screenWidth - MeasureText("PART 1 - ALL MODULES", 60)) / 2,
                     100, 60, DARKBLUE);

            DrawText("Shapes / Input / Text / Buttons / Movement",
                     (screenWidth - MeasureText("Shapes / Input / Text / Buttons / Movement", 30)) / 2,
                     180, 30, MAROON);

            // A few live values for feedback
            DrawText(TextFormat("FPS: %d", GetFPS()), 50, 700, 40,
                     GetFPS() >= 58 ? DARKGREEN : RED);

            DrawText(TextFormat("Shapes: %s", shapesVisible ? "ON" : "OFF"),
                     50, 750, 30, GRAY);

            // Draw the two shapes if enabled
            if (shapesVisible)
            {
                DrawRectangle(200, 350 + bob, 320, 180, (Color){180,0,255,255});
                DrawRectangleLines(200, 350 + bob, 320, 180, BLACK);

                DrawCircle(900, 440 + bob, 110, (Color){255,150,0,255});
                DrawCircleLines(900, 440 + bob, 110, BLACK);
            }

            // -------------------------------
            // Slider UI
            // -------------------------------
            DrawText("BOBBING SPEED", 520, 590, 25, DARKGRAY);

            DrawRectangleRec(sliderRect, LIGHTGRAY);
            DrawRectangleLinesEx(sliderRect, 3, DARKGRAY);

            // Slider knob position based on current speed
            float knobX = sliderRect.x +
                (bobSpeed - 0.5f) / 11.5f * sliderRect.width;

            DrawRectangle(knobX - 15, sliderRect.y - 10, 30, 50, DARKBLUE);
            DrawText(TextFormat("%.1f", bobSpeed),
                     sliderRect.x + sliderRect.width + 20, 615, 30, DARKBLUE);

            // Close button + bottom instructions
            closeBtn.Draw();

            DrawText("Press 1 or click to toggle - Drag slider - Hover/click CLOSE to exit",
                     200, 750, 24, DARKGRAY);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
