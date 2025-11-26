// =====================================================================
// Student Name      : Ariel Fajimiyo
// Student ID Number : C00300811@setu.ie
// Date              : 24/11/2025
// Purpose           : Demonstrates Raylib basics combined into one 
//                     interactive array visualizer. Includes animated
//                     buttons, selectable array cells, text input,
//                     sorting, shifting, deleting, and reset logic.
// =====================================================================

#include "raylib.h"
#include <vector>
#include <string>
#include <cmath>

// ---------------------------------------------------------
// Utility Easing + Helpers
// ---------------------------------------------------------

// Simple smooth cubic ease-out (0 → 1)
float EaseOutCubic(float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return 1.0f - powf(1.0f - t, 3.0f);
}

// Helper: color with modified alpha (0..1)
Color ColorWithAlpha(Color c, float alpha)
{
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    c.a = (unsigned char)(alpha * 255.0f);
    return c;
}

// ---------------------------------------------------------
// Cell Visual Data
// ---------------------------------------------------------
// logicalValue:   used by algorithms (bubble sort, shifts, delete)
// displayValue:   what the player currently sees inside the slot
// offsetX/Y:      animation offset (hover, swaps, etc.)
// overlayColor:   temporary highlight color (RED/GREEN/BLUE/ORANGE…)
// overlayAlpha:   visibility of overlay (0=transparent,1=solid)
// overlayTimer:   >=0 → countdown then fade, <0 → stays on (no fade)
// sortedLocked:   once bubble-sort knows this index is final
// ---------------------------------------------------------
struct Cell
{
    int   logicalValue   = 0;
    int   displayValue   = 0;

    float offsetX        = 0.0f;
    float offsetY        = 0.0f;

    Color baseColor      = LIGHTGRAY;

    Color overlayColor   = BLANK;
    float overlayAlpha   = 0.0f;
    float overlayTimer   = 0.0f;
    bool  overlayActive  = false;

    bool  sortedLocked   = false;
};

// ---------------------------------------------------------
// Global Animation Modes
// ---------------------------------------------------------
enum class GlobalAnimType
{
    None,
    Sort,
    ShiftLeft,
    ShiftRight,
    Delete
};

// ---------------------------------------------------------
// Bubble Sort Animation State
// ---------------------------------------------------------
// We visualise Bubble Sort step-by-step.
// For each pair (j, j+1):
//   CompareLift     → both ORANGE, hover up
//   CompareDecision → RED/GREEN (swap) or BLUE (stable)
//   SwapMove        → animated swap (if needed)
//   PostStep        → move to next pair / pass
// After each pass i, index (n-1-i) is locked (LIGHTGREEN).
// ---------------------------------------------------------
struct SortState
{
    bool active = false;

    int  i = 0;          // outer loop index
    int  j = 0;          // inner loop index
    int  n = 0;          // array size

    enum Phase
    {
        CompareLift,
        CompareDecision,
        SwapMove,
        PostStep
    } phase = CompareLift;

    float t = 0.0f;      // 0..1 time inside current phase
    bool  swapNeeded = false;
};

// ---------------------------------------------------------
// Shift / Delete as a sequence of adjacent swaps
// ---------------------------------------------------------
// We animate SHIFT LEFT, SHIFT RIGHT, and DELETE
// as a queue of neighbour swaps:
//   - SHIFT LEFT : (0,1), (1,2), ..., (N-2,N-1)
//   - SHIFT RIGHT: (N-1,N-2), (N-2,N-3), ..., (1,0)
//   - DELETE(s)  : (s,s+1), (s+1,s+2), ..., (N-2,N-1), then last→0
// Each swap:
//   - lifts both cells slightly
//   - flashes ORANGE on the two indices
//   - uses left/right horizontal movement logic
// ---------------------------------------------------------
struct SwapStep
{
    int a;
    int b;
};

struct ShiftSwapState
{
    bool active      = false;
    bool isDelete    = false;
    bool left        = true;     // logical direction (for info only)

    int  startIndex  = 0;        // used by delete

    std::vector<SwapStep> steps; // sequence of neighbour swaps
    int   currentStep = 0;
    float t           = 0.0f;    // 0..1 inside current swap
};

// ---------------------------------------------------------
// Overlay helpers
// ---------------------------------------------------------
// Start a one-shot or persistent overlay on a cell.
// duration > 0 → highlight, then fade automatically.
// duration < 0 → stay on (no fade) until manually reset.
// ---------------------------------------------------------
void TriggerOverlay(Cell& c, Color color, float duration)
{
    c.overlayColor  = color;
    c.overlayAlpha  = 1.0f;
    c.overlayActive = true;
    c.overlayTimer  = duration;   // <0 means infinite
}

void UpdateOverlay(Cell& c, float dt)
{
    if (!c.overlayActive)
        return;

    // Negative timer → persistent overlay (no fade)
    if (c.overlayTimer < 0.0f)
        return;

    // Count down the hold time first
    if (c.overlayTimer > 0.0f)
    {
        c.overlayTimer -= dt;
        if (c.overlayTimer < 0.0f)
            c.overlayTimer = 0.0f;
    }
    else
    {
        // Then fade alpha down once
        c.overlayAlpha -= dt * 2.0f;
        if (c.overlayAlpha <= 0.0f)
        {
            c.overlayAlpha  = 0.0f;
            c.overlayActive = false;
        }
    }
}

// ---------------------------------------------------------
// MAIN
// ---------------------------------------------------------
int main()
{
    InitWindow(1100, 720, "Array Visualizer");
    SetTargetFPS(60);

    // -------------------------------------------------------------
    // Array / cell setup
    // -------------------------------------------------------------
    const int ARRAY_SIZE = 8;
    std::vector<Cell> cells(ARRAY_SIZE);

    // Text input and selection state
    int         selectedIndex = -1;
    bool        editing       = false;
    std::string inputBuffer   = "";

    // Button animation (squish on press)
    float squishT           = 0.0f;
    int   lastPressedButton = -1; // -1 none, 0 sort,1 delete,2 shiftL,3 shiftR,4 reset,-2 enter

    // Layout constants
    const float boxW    = 95.0f;
    const float boxH    = 95.0f;
    const float padding = 28.0f;

    // Animation / algorithm state
    GlobalAnimType currentAnim   = GlobalAnimType::None;
    SortState      sortState;
    ShiftSwapState shiftState;

    // Compute a slot's base position (index → screen)
    auto GetSlotBasePos = [&](int index) -> Vector2
    {
        float totalW = ARRAY_SIZE * (boxW + padding) - padding;
        float startX = GetScreenWidth() / 2.0f - totalW / 2.0f;
        float y      = GetScreenHeight() * 0.40f;
        return { startX + index * (boxW + padding), y };
    };

    // Initialise all cells to zero values and default visuals
    for (int i = 0; i < ARRAY_SIZE; ++i)
    {
        cells[i].logicalValue  = 0;
        cells[i].displayValue  = 0;
        cells[i].baseColor     = LIGHTGRAY;
        cells[i].overlayActive = false;
        cells[i].overlayAlpha  = 0.0f;
        cells[i].sortedLocked  = false;
        cells[i].offsetX       = 0.0f;
        cells[i].offsetY       = 0.0f;
    }

    // -------------------------------------------------------------
    // Helper lambdas for controlling algorithm animations
    // -------------------------------------------------------------

    // Reset all animation-related visuals (keeps actual values)
    auto ClearAlgorithmVisuals = [&]()
    {
        for (int i = 0; i < ARRAY_SIZE; ++i)
        {
            cells[i].offsetX       = 0.0f;
            cells[i].offsetY       = 0.0f;
            cells[i].overlayActive = false;
            cells[i].overlayAlpha  = 0.0f;
            cells[i].overlayTimer  = 0.0f;
            cells[i].sortedLocked  = false;
        }
    };

    // Start bubble sort animation
    auto StartSortAnimation = [&]()
    {
        if (currentAnim != GlobalAnimType::None)
            return;

        currentAnim          = GlobalAnimType::Sort;
        sortState.active     = true;
        sortState.i          = 0;
        sortState.j          = 0;
        sortState.n          = ARRAY_SIZE;
        sortState.t          = 0.0f;
        sortState.phase      = SortState::CompareLift;
        sortState.swapNeeded = false;

        ClearAlgorithmVisuals();
    };

    // Build neighbour swap steps for shift-left / shift-right / delete
    auto BuildShiftSteps = [&](bool left, bool isDelete, int startIndex)
    {
        shiftState.steps.clear();
        shiftState.currentStep = 0;
        shiftState.t           = 0.0f;
        shiftState.active      = true;
        shiftState.left        = left;
        shiftState.isDelete    = isDelete;
        shiftState.startIndex  = startIndex;

        if (!isDelete)
        {
            if (left)
            {
                // SHIFT LEFT: swap (0,1), (1,2), ..., (N-2,N-1)
                for (int i = 0; i < ARRAY_SIZE - 1; ++i)
                {
                    shiftState.steps.push_back({ i, i + 1 });
                }
            }
            else
            {
                // SHIFT RIGHT: swap (N-1,N-2), (N-2,N-3), ..., (1,0)
                for (int i = ARRAY_SIZE - 1; i > 0; --i)
                {
                    shiftState.steps.push_back({ i, i - 1 });
                }
            }
        }
        else
        {
            // DELETE: swap (s,s+1), (s+1,s+2), ..., (N-2,N-1), then clear last
            for (int i = startIndex; i < ARRAY_SIZE - 1; ++i)
            {
                shiftState.steps.push_back({ i, i + 1 });
            }
        }
    };

    auto StartShiftLeft = [&]()
    {
        if (currentAnim != GlobalAnimType::None)
            return;

        currentAnim = GlobalAnimType::ShiftLeft;
        ClearAlgorithmVisuals();
        BuildShiftSteps(true, false, 0);

        // Index 0 is the origin of the left shift → stay solid RED
        TriggerOverlay(cells[0], RED, -1.0f); // -1 = no fade
    };

    auto StartShiftRight = [&]()
    {
        if (currentAnim != GlobalAnimType::None)
            return;

        currentAnim = GlobalAnimType::ShiftRight;
        ClearAlgorithmVisuals();
        BuildShiftSteps(false, false, 0);

        // Symmetric: last index as origin of right shift → stay RED
        TriggerOverlay(cells[ARRAY_SIZE - 1], RED, -1.0f);
    };

auto StartDeleteAnimation = [&](int fromIndex)
{
    if (currentAnim != GlobalAnimType::None) return;
    if (fromIndex < 0 || fromIndex >= ARRAY_SIZE) return;

    currentAnim = GlobalAnimType::Delete;
    ClearAlgorithmVisuals();
    BuildShiftSteps(true, true, fromIndex);

    // Just a short flash — don't leave red forever
    TriggerOverlay(cells[fromIndex], RED, 0.6f);
};

    // -------------------------------------------------------------
    // Main loop
    // -------------------------------------------------------------
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        Vector2 mouse = GetMousePosition();

        float totalW = ARRAY_SIZE * (boxW + padding) - padding;
        float startX = GetScreenWidth() / 2.0f - totalW / 2.0f;
        float y      = GetScreenHeight() * 0.40f;

        // Button layout
        float btnY = y + boxH + 120.0f;
        float sx   = startX;

        Rectangle btnSort   = { sx,          btnY, 140, 60 };
        Rectangle btnDelete = { sx + 160.0f, btnY, 140, 60 };
        Rectangle btnShiftL = { sx + 320.0f, btnY, 140, 60 };
        Rectangle btnShiftR = { sx + 480.0f, btnY, 140, 60 };
        Rectangle btnReset  = { sx + 640.0f, btnY, 140, 60 };

        // ---------------------------------------------------------
        // UPDATE OVERLAYS (fade once, no looping pulses)
        // ---------------------------------------------------------
        for (int i = 0; i < ARRAY_SIZE; ++i)
        {
            UpdateOverlay(cells[i], dt);
        }

        // ---------------------------------------------------------
        // SORT ANIMATION UPDATE (Bubble Sort)
        // ---------------------------------------------------------
        if (currentAnim == GlobalAnimType::Sort && sortState.active)
        {
            SortState &S = sortState;

            int j  = S.j;
            int jp = S.j + 1;
            int n  = S.n;

            // Safety check
            if (j < 0 || jp >= n)
            {
                S.active    = false;
                currentAnim = GlobalAnimType::None;
            }
            else
            {
                // Reset offsets for all non-active cells
                for (int i = 0; i < ARRAY_SIZE; ++i)
                {
                    if (i != j && i != jp)
                    {
                        cells[i].offsetX = 0.0f;
                        cells[i].offsetY = 0.0f;
                    }
                }

                const float LIFT_TIME     = 0.25f;
                const float DECISION_TIME = 0.35f;
                const float SWAP_TIME     = 0.45f;
                const float POST_TIME     = 0.40f;

                if (S.phase == SortState::CompareLift)
                {
                    S.t += dt / LIFT_TIME;
                    float e = EaseOutCubic(S.t);

                    // Hover both cells up while painting them ORANGE
                    cells[j ].offsetY = -18.0f * e;
                    cells[jp].offsetY = -18.0f * e;

                    TriggerOverlay(cells[j ], ORANGE, 0.3f);
                    TriggerOverlay(cells[jp], ORANGE, 0.3f);

                    if (S.t >= 1.0f)
                    {
                        S.t          = 0.0f;
                        S.phase      = SortState::CompareDecision;
                        S.swapNeeded = (cells[j].logicalValue > cells[jp].logicalValue);
                    }
                }
                else if (S.phase == SortState::CompareDecision)
                {
                    S.t += dt / DECISION_TIME;
                    float e = EaseOutCubic(S.t);
                    (void)e; // currently unused, but left for tweakable easing

                    // Color logic:
                    // If left > right → LEFT RED, RIGHT GREEN (swap needed)
                    // Else (<=)      → both BLUE (stable)
                    if (S.swapNeeded)
                    {
                        TriggerOverlay(cells[j ], RED,   0.35f);
                        TriggerOverlay(cells[jp], GREEN, 0.35f);
                    }
                    else
                    {
                        TriggerOverlay(cells[j ], BLUE, 0.35f);
                        TriggerOverlay(cells[jp], BLUE, 0.35f);
                    }

                    if (S.t >= 1.0f)
                    {
                        S.t = 0.0f;
                        if (S.swapNeeded)
                        {
                            S.phase = SortState::SwapMove;
                        }
                        else
                        {
                            // No swap → go straight to post step
                            S.phase = SortState::PostStep;
                        }
                    }
                }
                else if (S.phase == SortState::SwapMove)
                {
                    S.t += dt / SWAP_TIME;
                    float e = EaseOutCubic(S.t);

                    float dx = (boxW + padding);

                    // Animate them horizontally crossing with slight lift down
                    cells[j ].offsetX =  dx * e;
                    cells[jp].offsetX = -dx * e;

                    // Return to baseline vertically
                    cells[j ].offsetY = -18.0f * (1.0f - e);
                    cells[jp].offsetY = -18.0f * (1.0f - e);

                    if (S.t >= 1.0f)
                    {
                        // Commit the swap of actual values
                        std::swap(cells[j ].logicalValue, cells[jp].logicalValue);
                        std::swap(cells[j ].displayValue, cells[jp].displayValue);

                        // Reset offsets back to rest
                        cells[j ].offsetX = 0.0f;
                        cells[j ].offsetY = 0.0f;
                        cells[jp].offsetX = 0.0f;
                        cells[jp].offsetY = 0.0f;

                        // New position (smaller value) flashes GREEN
                        // Old position (displaced) flashes RED
                        TriggerOverlay(cells[j ], GREEN, 0.4f);
                        TriggerOverlay(cells[jp], RED,   0.4f);

                        S.t     = 0.0f;
                        S.phase = SortState::PostStep;
                    }
                }
                else if (S.phase == SortState::PostStep)
                {
                    S.t += dt / POST_TIME;

                    if (S.t >= 1.0f)
                    {
                        S.t++;

                        // Advance inner loop
                        S.j++;

                        // If inner loop finished for this pass i
                        if (S.j >= (n - 1 - S.i))
                        {
                            // Element at (n-1-i) is now fixed. Lock it.
                            int sortedIndex = n - 1 - S.i;
                            cells[sortedIndex].sortedLocked = true;
                            TriggerOverlay(cells[sortedIndex], Color{144, 238, 144, 255}, 0.7f); // LIGHT GREEN

                            // Next pass
                            S.i++;
                            S.j = 0;

                            if (S.i >= n - 1)
                            {
                                // Final element also sorted
                                cells[0].sortedLocked = true;
                                TriggerOverlay(cells[0], Color{144, 238, 144, 255}, 0.7f);

                                // Reset all offsets to ensure nothing stays lifted
                                for (int k = 0; k < ARRAY_SIZE; ++k)
                                {
                                    cells[k].offsetX = 0.0f;
                                    cells[k].offsetY = 0.0f;
                                }

                                S.active    = false;
                                currentAnim = GlobalAnimType::None;
                            }
                            else
                            {
                                // Continue with next comparison
                                S.phase = SortState::CompareLift;
                                S.t     = 0.0f;
                            }
                        }
                        else
                        {
                            // Not at end of pass yet → go to next pair
                            S.phase = SortState::CompareLift;
                            S.t     = 0.0f;
                        }
                    }
                }
            }
        }

        // ---------------------------------------------------------
        // SHIFT / DELETE ANIMATION UPDATE (sequence of swaps)
        // ---------------------------------------------------------
        if ((currentAnim == GlobalAnimType::ShiftLeft ||
             currentAnim == GlobalAnimType::ShiftRight ||
             currentAnim == GlobalAnimType::Delete) &&
            shiftState.active)
        {
            ShiftSwapState &SH = shiftState;
            const float SWAP_TIME = 0.4f;

            // No steps → nothing to do
            if (SH.currentStep >= (int)SH.steps.size())
            {
                // Special case for delete: last cell becomes 0
                if (SH.isDelete)
                {
                    cells[ARRAY_SIZE - 1].logicalValue = 0;
                    cells[ARRAY_SIZE - 1].displayValue = 0;
                    TriggerOverlay(cells[ARRAY_SIZE - 1], DARKGRAY, 0.8f);
                }

                // Clear persistent overlays (e.g. the red origin index)
                for (int i = 0; i < ARRAY_SIZE; ++i)
                {
                    if (cells[i].overlayTimer < 0.0f)
                    {
                        // Manually turn off infinite overlays at the end
                        cells[i].overlayTimer = 0.0f;
                        cells[i].overlayAlpha = 0.0f;
                        cells[i].overlayActive = false;
                    }
                }

                SH.active    = false;
                currentAnim  = GlobalAnimType::None;

                // Make sure all offsets are zero
                for (int i = 0; i < ARRAY_SIZE; ++i)
                {
                    cells[i].offsetX = 0.0f;
                    cells[i].offsetY = 0.0f;
                }
            }
            else
            {
                // We are in the middle of a neighbour swap
                SwapStep step = SH.steps[SH.currentStep];
                int a = step.a;
                int b = step.b;

                // Reset offsets each frame, then apply only to active pair
                for (int i = 0; i < ARRAY_SIZE; ++i)
                {
                    cells[i].offsetX = 0.0f;
                    cells[i].offsetY = 0.0f;
                }

                SH.t += dt / SWAP_TIME;
                float e = EaseOutCubic(SH.t);

                int dir = (b > a) ? 1 : -1;  // movement direction
                float dx = (boxW + padding);

                // Flash ORANGE on the two cells being swapped
                TriggerOverlay(cells[a], ORANGE, 0.2f);
                TriggerOverlay(cells[b], ORANGE, 0.2f);

                // Movement logic: cross horizontally with a small lift
                cells[a].offsetX =  dx * e * dir;
                cells[b].offsetX = -dx * e * dir;
                cells[a].offsetY = -14.0f * (1.0f - e);
                cells[b].offsetY = -14.0f * (1.0f - e);

                if (SH.t >= 1.0f)
                {
                    // Commit actual values swap
                    std::swap(cells[a].logicalValue, cells[b].logicalValue);
                    std::swap(cells[a].displayValue, cells[b].displayValue);

                    // Reset offsets to rest
                    cells[a].offsetX = 0.0f;
                    cells[a].offsetY = 0.0f;
                    cells[b].offsetX = 0.0f;
                    cells[b].offsetY = 0.0f;

                    // Step finished → next
                    SH.currentStep++;
                    SH.t = 0.0f;
                }
            }
        }

        // ---------------------------------------------------------
        // INPUT: Interaction (disabled during algorithm animations)
        // ---------------------------------------------------------
        bool animationBusy = (currentAnim != GlobalAnimType::None);

        // -----------------------------
        // Click to select a cell
        // -----------------------------
        if (!animationBusy && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            bool hit = false;
            for (int i = 0; i < ARRAY_SIZE; ++i)
            {
                Vector2 basePos  = GetSlotBasePos(i);
                Rectangle r      = { basePos.x, basePos.y, boxW, boxH };

                if (CheckCollisionPointRec(mouse, r))
                {
                    selectedIndex = i;
                    editing       = true;

                    if (cells[i].logicalValue == 0)
                        inputBuffer.clear();
                    else
                        inputBuffer = std::to_string(cells[i].logicalValue);

                    // Red flash on selection (one-shot, no loop)
                    TriggerOverlay(cells[i], RED, 0.4f);

                    hit = true;
                }
            }
            if (!hit)
            {
                selectedIndex = -1;
                editing       = false;
                inputBuffer.clear();
            }
        }

        // -----------------------------
        // Typing digits into selected cell
        // -----------------------------
        if (!animationBusy && editing)
        {
            int key = GetCharPressed();
            while (key > 0)
            {
                if (key >= '0' && key <= '9' && inputBuffer.size() < 5)
                {
                    inputBuffer.push_back((char)key);
                }
                key = GetCharPressed();
            }

            if (IsKeyPressed(KEY_BACKSPACE) && !inputBuffer.empty())
            {
                inputBuffer.pop_back();
            }

            if (IsKeyPressed(KEY_ENTER))
            {
                int newVal = 0;
                if (!inputBuffer.empty())
                    newVal = std::stoi(inputBuffer);

                if (selectedIndex >= 0 && selectedIndex < ARRAY_SIZE)
                {
                    cells[selectedIndex].logicalValue = newVal;
                    cells[selectedIndex].displayValue = newVal;

                    // Small commit highlight
                    lastPressedButton = -2;
                    squishT           = 1.0f;
                    TriggerOverlay(cells[selectedIndex], GREEN, 0.4f);
                }

                editing       = false;
                selectedIndex = -1;
                inputBuffer.clear();
            }
        }

        // ---------------------------------------------------------
        // BUTTON CLICK LOGIC (disabled while algorithms running)
        // ---------------------------------------------------------
        if (!animationBusy && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (CheckCollisionPointRec(mouse, btnSort))
            {
                lastPressedButton = 0;
                squishT           = 1.0f;
                StartSortAnimation();
            }
            else if (CheckCollisionPointRec(mouse, btnDelete))
            {
                lastPressedButton = 1;
                squishT           = 1.0f;

                if (selectedIndex != -1)
                {
                    StartDeleteAnimation(selectedIndex);
                    selectedIndex = -1;
                    editing       = false;
                    inputBuffer.clear();
                }
            }
            else if (CheckCollisionPointRec(mouse, btnShiftL))
            {
                lastPressedButton = 2;
                squishT           = 1.0f;
                StartShiftLeft();
            }
            else if (CheckCollisionPointRec(mouse, btnShiftR))
            {
                lastPressedButton = 3;
                squishT           = 1.0f;
                StartShiftRight();
            }
            else if (CheckCollisionPointRec(mouse, btnReset))
            {
                lastPressedButton = 4;
                squishT           = 1.0f;

                for (int i = 0; i < ARRAY_SIZE; ++i)
                {
                    cells[i].logicalValue  = 0;
                    cells[i].displayValue  = 0;
                    cells[i].offsetX       = 0.0f;
                    cells[i].offsetY       = 0.0f;
                    cells[i].overlayActive = false;
                    cells[i].overlayAlpha  = 0.0f;
                    cells[i].overlayTimer  = 0.0f;
                    cells[i].sortedLocked  = false;
                }
                selectedIndex     = -1;
                editing           = false;
                inputBuffer.clear();
                currentAnim       = GlobalAnimType::None;
                sortState.active  = false;
                shiftState.active = false;
            }
        }

        // ---------------------------------------------------------
        // BUTTON SQUISH DECAY
        // ---------------------------------------------------------
        if (squishT > 0.0f)
        {
            squishT -= dt * 3.0f;
            if (squishT < 0.0f) squishT = 0.0f;
        }

        // ---------------------------------------------------------
        // DRAW
        // ---------------------------------------------------------
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Array Visualizer", GetScreenWidth()/2 - 190, 45, 42, DARKBLUE);
        DrawText("Click cell → type digits → ENTER to apply",
                 GetScreenWidth()/2 - 240, 110, 22, DARKGRAY);

        bool animationBusyDraw = (currentAnim != GlobalAnimType::None);

        // ----------------------------------
        // Draw array slots and cell values
        // ----------------------------------
        for (int i = 0; i < ARRAY_SIZE; ++i)
        {
            Vector2 basePos = GetSlotBasePos(i);

            float drawX = basePos.x + cells[i].offsetX;
            float drawY = basePos.y + cells[i].offsetY;

            Rectangle r = { drawX, drawY, boxW, boxH };

            // Slot background
            DrawRectangleRec(r, cells[i].baseColor);

            // Hover outline (only when not animating algorithms)
            if (!animationBusyDraw && CheckCollisionPointRec(mouse, r))
                DrawRectangleLinesEx(r, 3, SKYBLUE);
            else
                DrawRectangleLinesEx(r, 3, BLACK);

            // Overlay highlight (ORANGE/RED/GREEN/BLUE/LIGHTGREEN)
            if (cells[i].overlayActive && cells[i].overlayAlpha > 0.0f)
            {
                Color c = ColorWithAlpha(cells[i].overlayColor, cells[i].overlayAlpha);
                DrawRectangleRec(r, c);
                DrawRectangleLinesEx(r, 3, cells[i].overlayColor);
            }

            // Index number above the box (does not move)
            std::string idx = std::to_string(i);
            DrawText(idx.c_str(),
                     basePos.x + boxW/2 - MeasureText(idx.c_str(), 20)/2,
                     basePos.y - 28,
                     20,
                     DARKGRAY);

            // Show either displayValue or current input buffer
            if (editing && i == selectedIndex)
            {
                std::string str = inputBuffer.empty() ? "" : inputBuffer;
                DrawText(str.c_str(),
                         r.x + boxW/2 - MeasureText(str.c_str(), 30)/2,
                         r.y + boxH/2 - 15,
                         30,
                         BLACK);
            }
            else
            {
                int v = cells[i].displayValue;
                if (v != 0)
                {
                    std::string val = std::to_string(v);
                    DrawText(val.c_str(),
                             r.x + boxW/2 - MeasureText(val.c_str(), 30)/2,
                             r.y + boxH/2 - 15,
                             30,
                             BLACK);
                }
            }
        }

        // =====================================================================
        // BUTTON DRAWER WITH SQUISH (non-looping on press)
        // =====================================================================
        auto DrawFancyButton = [&](Rectangle r, Color color, const char* label, int id)
        {
            float scale = 1.0f;

            if (lastPressedButton == id)
            {
                float e = EaseOutCubic(squishT);
                scale   = 1.0f - 0.15f * e;
            }

            float cx = r.x + r.width  / 2.0f;
            float cy = r.y + r.height / 2.0f;

            float w  = r.width  * scale;
            float h  = r.height * scale;

            Rectangle anim = { cx - w / 2.0f, cy - h / 2.0f, w, h };

            DrawRectangleRec(anim, color);
            DrawRectangleLinesEx(anim, 3, BLACK);

            int tw = MeasureText(label, 20);
            DrawText(label,
                     anim.x + w / 2.0f - tw / 2.0f,
                     anim.y + h / 2.0f - 12,
                     20,
                     WHITE);
        };

        DrawFancyButton(btnSort,   BLUE,   "SORT",     0);
        DrawFancyButton(btnShiftL, ORANGE, "SHIFT L",  1);
        DrawFancyButton(btnShiftR, PURPLE, "SHIFT R",  2);
        DrawFancyButton(btnReset,  GREEN,  "RESET",    3);

        // =====================================================================
        // Selected info text
        // =====================================================================
        DrawText("Selected Index:", startX, btnY + 80, 22, DARKGRAY);
        if (selectedIndex != -1)
        {
            DrawText(TextFormat("%d", selectedIndex),
                     startX + 190, btnY + 80, 22, BLACK);

            int v = cells[selectedIndex].logicalValue;
            const char* valText = (v == 0 ? "(empty)" : TextFormat("%d", v));

            DrawText("Value:", startX + 260, btnY + 80, 22, DARKGRAY);
            DrawText(valText, startX + 350, btnY + 80, 22, BLACK);
        }
        else
        {
            DrawText("None", startX + 190, btnY + 80, 22, BLACK);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
