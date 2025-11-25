#include "raylib.h"
#include <vector>
#include <string>
#include <cmath>

// ---------------------------------------------------------
// Array Visualizer – Single File
// Click a cell → it flashes red → user types → ENTER saves
// ---------------------------------------------------------

int main() {
    InitWindow(1000, 650, "Array Visualizer");
    SetTargetFPS(60);

    // -----------------------------
    // Array Data
    // -----------------------------
    std::vector<int> arr = { 5, 9, 12, 4, 8 };
    int selectedIndex = -1;

    // -----------------------------
    // Text Input State
    // -----------------------------
    bool editing = false;
    std::string inputBuffer = "";

    // -----------------------------
    // Flash Animation
    // -----------------------------
    float flashTime = 0.0f;

    // -----------------------------
    // Layout
    // -----------------------------
    const float boxW = 90;
    const float boxH = 90;
    const float padding = 25;

    while (!WindowShouldClose()) {

        // =====================================================
        // UPDATE
        // =====================================================
        flashTime += GetFrameTime();
        Vector2 mouse = GetMousePosition();

        // Compute centering
        float totalW = arr.size() * (boxW + padding) - padding;
        float startX = GetScreenWidth() / 2.0f - totalW / 2.0f;
        float y = GetScreenHeight() * 0.40f;

        // ----------------------------------------
        // Detect Click on Array Cell
        // ----------------------------------------
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            bool hitCell = false;

            for (int i = 0; i < arr.size(); i++) {
                Rectangle r = {
                    startX + i * (boxW + padding),
                    y,
                    boxW,
                    boxH
                };

                if (CheckCollisionPointRec(mouse, r)) {
                    selectedIndex = i;
                    editing = true;
                    inputBuffer = std::to_string(arr[i]);  // preload old value
                    hitCell = true;
                }
            }

            if (!hitCell) {
                editing = false;
                selectedIndex = -1;
            }
        }

        // ----------------------------------------
        // Typing Input (only digits)
        // ----------------------------------------
        if (editing) {
            int key = GetCharPressed();
            while (key > 0) {
                if (key >= '0' && key <= '9') {
                    if (inputBuffer.size() < 6)  // avoid huge numbers
                        inputBuffer.push_back((char)key);
                }
                key = GetCharPressed();
            }

            if (IsKeyPressed(KEY_BACKSPACE) && !inputBuffer.empty()) {
                inputBuffer.pop_back();
            }

            // Finish editing
            if (IsKeyPressed(KEY_ENTER)) {
                if (!inputBuffer.empty())
                    arr[selectedIndex] = std::stoi(inputBuffer);

                editing = false;
                selectedIndex = -1;
                inputBuffer = "";
            }
        }

        // =====================================================
        // DRAW
        // =====================================================
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Array Visualizer", GetScreenWidth()/2 - 180, 50, 40, DARKBLUE);
        DrawText("Click a cell → it flashes red → type → ENTER", 
                 GetScreenWidth()/2 - 240, 110, 20, GRAY);

        for (int i = 0; i < arr.size(); i++) {

            float x = startX + i * (boxW + padding);

            Rectangle cell = { x, y, boxW, boxH };

            // ----------------------------------------
            // Base background
            // ----------------------------------------
            DrawRectangleRec(cell, LIGHTGRAY);

            // ----------------------------------------
            // Hover outline
            // ----------------------------------------
            if (CheckCollisionPointRec(mouse, cell))
                DrawRectangleLinesEx(cell, 3, SKYBLUE);

            // ----------------------------------------
            // Selected flashing (red pulse)
            // ----------------------------------------
            if (i == selectedIndex) {
                float pulse = (sinf(flashTime * 6) + 1) * 0.5f; // 0–1
                Color flash = { 255, 50, 50, (unsigned char)(100 + pulse * 100) };
                DrawRectangleRec(cell, flash);
                DrawRectangleLinesEx(cell, 4, RED);
            } else {
                DrawRectangleLinesEx(cell, 3, BLACK);
            }

            // ----------------------------------------
            // Index above cell
            // ----------------------------------------
            std::string idx = std::to_string(i);
            DrawText(idx.c_str(),
                     x + boxW/2 - MeasureText(idx.c_str(), 20)/2,
                     y - 30,
                     20,
                     DARKGRAY);

            // ----------------------------------------
            // Value inside cell
            // ----------------------------------------
            std::string val =
                (editing && i == selectedIndex) ? inputBuffer : std::to_string(arr[i]);

            DrawText(val.c_str(),
                     x + boxW/2 - MeasureText(val.c_str(), 30)/2,
                     y + boxH/2 - 15,
                     30,
                     BLACK);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
