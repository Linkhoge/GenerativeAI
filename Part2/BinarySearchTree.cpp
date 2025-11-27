#include "raylib.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <ctime>
#include <algorithm>
using namespace std;

// ============================================================
// NODE STRUCT
// ============================================================

struct Node {
    int key;
    Node* left;
    Node* right;

    float x, y;
    float targetX, targetY;

    unsigned char alpha;

    Node(int k) {
        key = k;
        left = right = nullptr;
        x = y = 0;
        targetX = targetY = 0;
        alpha = 0;
    }
};

Node* root = nullptr;

// ============================================================
// GLOBALS (SELECTION, SEARCH, DELETE, VISUALIZE, CAMERA)
// ============================================================

Node* selectedNode = nullptr;
Node* deleteTargetNode = nullptr;

bool deleteAnimationActive = false;
float deleteTimer = 0.0f;

float globalTime = 0.0f;

// ---- SEARCH ----
vector<Node*> searchPath;
float searchTimer = 0.0f;
int searchIndex = -1;
bool searchActive = false;
bool searchFound = false;

// ---- CAMERA ----
float camZoom = 1.0f;
float camZoomTarget = 1.0f;

// ---- VISUALIZE ----
vector<int> visualizeSeq;
int visualizeIndex = -1;
bool visualizeActive = false;
float visualizeTimer = 0.0f;

// ============================================================
// CAMERA SCREEN→WORLD
// ============================================================

Vector2 ScreenToWorld(Camera2D cam, Vector2 screenPos) {
    float x = (screenPos.x - cam.offset.x) / cam.zoom + cam.target.x;
    float y = (screenPos.y - cam.offset.y) / cam.zoom + cam.target.y;
    return {x, y};
}

// ============================================================
// BST LOGIC
// ============================================================

Node* insertRec(Node* n, int key) {
    if (!n) return new Node(key);
    if (key < n->key) n->left = insertRec(n->left, key);
    else if (key > n->key) n->right = insertRec(n->right, key);
    return n;
}

Node* findMin(Node* n) {
    while (n && n->left) n = n->left;
    return n;
}

Node* removeRec(Node* n, int key) {
    if (!n) return nullptr;

    if (key < n->key) n->left = removeRec(n->left, key);
    else if (key > n->key) n->right = removeRec(n->right, key);
    else {
        if (!n->left && !n->right) { delete n; return nullptr; }
        if (!n->left) { Node* r = n->right; delete n; return r; }
        if (!n->right) { Node* l = n->left; delete n; return l; }

        Node* s = findMin(n->right);
        n->key = s->key;
        n->right = removeRec(n->right, s->key);
    }
    return n;
}

Node* searchRecord(Node* n, int key) {
    searchPath.clear();
    Node* cur = n;

    while (cur) {
        searchPath.push_back(cur);
        if (cur->key == key) return cur;
        if (key < cur->key) cur = cur->left;
        else cur = cur->right;
    }
    return nullptr;
}

// ============================================================
// LAYOUT
// ============================================================

void computeLayout(Node* n, float x, float y, float spacing) {
    if (!n) return;

    n->targetX = x;
    n->targetY = y;

    computeLayout(n->left,  x - spacing, y + 80, spacing * 0.5f);
    computeLayout(n->right, x + spacing, y + 80, spacing * 0.5f);
}

// ============================================================
// ANIMATION
// ============================================================

void updatePositions(Node* n) {
    if (!n) return;

    if (n->alpha < 255) n->alpha += 4;

    n->x += (n->targetX - n->x) * 0.15f;
    n->y += (n->targetY - n->y) * 0.15f;

    updatePositions(n->left);
    updatePositions(n->right);
}

// ============================================================
// INFO PANEL HELPERS
// ============================================================

Node* findParent(Node* root, Node* t, Node* parent = nullptr) {
    if (!root) return nullptr;
    if (root == t) return parent;

    Node* L = findParent(root->left, t, root);
    if (L) return L;

    return findParent(root->right, t, root);
}

int nodeHeight(Node* n) {
    if (!n) return -1;
    return 1 + max(nodeHeight(n->left), nodeHeight(n->right));
}

bool isLeaf(Node* n) {
    return n && !n->left && !n->right;
}

// ============================================================
// DRAW HELPERS
// ============================================================

Color blend(Color a, Color b, float t) {
    return {
        (unsigned char)(a.r + (b.r - a.r)*t),
        (unsigned char)(a.g + (b.g - a.g)*t),
        (unsigned char)(a.b + (b.b - a.b)*t),
        255
    };
}

void drawEdges(Node* n) {
    if (!n) return;

    if (n->left)  DrawLine(n->x, n->y, n->left->x, n->left->y, DARKGRAY);
    if (n->right) DrawLine(n->x, n->y, n->right->x, n->right->y, DARKGRAY);

    drawEdges(n->left);
    drawEdges(n->right);
}

void drawNodes(Node* n) {
    if (!n) return;

    Color base = Color{200,200,200,n->alpha};
    Color col = base;
    bool highlighted = false;

    // Selected node flashing
    if (n == selectedNode && !searchActive && !deleteAnimationActive) {
        float t = (sinf(globalTime * 6) + 1) / 2;
        col = blend(base, RED, t);
        highlighted = true;
    }

    // Search animation
    if (searchActive && !highlighted) {
        for (int i = 0; i <= searchIndex; i++)
            if (searchPath[i] == n)
                col = blend(base, Color{255,150,0,255}, 0.6f);

        if (searchIndex == searchPath.size()-1) {
            if (searchFound && n == searchPath.back()) col = Color{0,255,0,255};
            if (!searchFound && n == searchPath.back()) col = RED;
        }
    }

    // Delete animation
    if (deleteAnimationActive && deleteTargetNode == n)
        col = RED;

    DrawCircle(n->x, n->y, 24, col);
    DrawCircleLines(n->x, n->y, 24, BLACK);
    DrawText(TextFormat("%d", n->key), n->x - 10, n->y - 10, 20, BLACK);

    drawNodes(n->left);
    drawNodes(n->right);
}

// ============================================================
// BUTTON STRUCT
// ============================================================

struct UIButton {
    Rectangle rect;
    float scale;
    float animSpeed;
    Color color;
    const char* label;
};

bool DrawUIButton(UIButton &b) {
    bool pressed = false;

    Vector2 m = GetMousePosition();
    bool hover = CheckCollisionPointRec(m, b.rect);

    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        b.scale = 0.9f;
        pressed = true;
    }

    b.scale += (1 - b.scale) * b.animSpeed;

    float w = b.rect.width  * b.scale;
    float h = b.rect.height * b.scale;

    float x = b.rect.x + (b.rect.width  - w) / 2;
    float y = b.rect.y + (b.rect.height - h) / 2;

    DrawRectangle(x,y,w,h,b.color);
    DrawRectangleLines(x,y,w,h,BLACK);
    DrawText(b.label, x+10,y+8,20,BLACK);

    return pressed;
}

// ============================================================
// NODE PICKING (WORLD SPACE)
// ============================================================

void pickNode(Node* n, Vector2 m, float r, Node*& out) {
    if (!n) return;

    if ((m.x - n->x)*(m.x - n->x) + (m.y - n->y)*(m.y - n->y) <= r*r)
        out = n;

    pickNode(n->left, m, r, out);
    pickNode(n->right, m, r, out);
}

// ============================================================
// MAIN
// ============================================================

int main() {

    InitWindow(1400, 900, "BST Visualisation");
    SetTargetFPS(60);

    // Random seed for true randomness
    SetRandomSeed((unsigned int)time(NULL));

    int inputValue = 0;

    UIButton insertBtn    = {{20,40,120,40},1,0.18f,Color{120,230,120,255},"Insert"};
    UIButton deleteBtn    = {{20,90,120,40},1,0.18f,Color{230,120,120,255},"Delete"};
    UIButton searchBtn    = {{20,140,120,40},1,0.18f,Color{120,160,230,255},"Search"};
    UIButton visualizeBtn = {{20,190,120,40},1,0.18f,Color{255,200,0,255},"Visualize"};

    Rectangle inputBox = {20,240,120,40};

    while (!WindowShouldClose()) {

        float dt = GetFrameTime();
        globalTime += dt;

        // -------------------------------
        // CAMERA ZOOM
        // -------------------------------
        if (IsKeyDown(KEY_I)) camZoomTarget += 0.02f;
        if (IsKeyDown(KEY_O)) camZoomTarget -= 0.02f;

        camZoomTarget = (camZoomTarget < 0.3f) ? 0.3f : (camZoomTarget > 3.0f) ? 3.0f : camZoomTarget;
        camZoom += (camZoomTarget - camZoom) * 0.1f;

        // Camera configuration
        Camera2D cam;
        cam.offset = {(float)GetScreenWidth()/2, (float)GetScreenHeight()/2};
        cam.target = {700,200};
        cam.rotation = 0;
        cam.zoom = camZoom;

        // =====================================================
        // DRAW START
        // =====================================================
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("BST Visualisation", 20, 10, 26, BLACK);

        // -------------------------------
        // INPUT BOX
        // -------------------------------
        DrawRectangleRec(inputBox, LIGHTGRAY);
        DrawRectangleLines(inputBox.x,inputBox.y,inputBox.width,inputBox.height,BLACK);
        DrawText(TextFormat("%d", inputValue), inputBox.x+10,inputBox.y+8,20,BLACK);

        int key = GetKeyPressed();
        if (key >= KEY_ZERO && key <= KEY_NINE)
            inputValue = inputValue*10 + (key - KEY_ZERO);
        if (key == KEY_BACKSPACE && !selectedNode)
            inputValue /= 10;

        // -------------------------------
        // UI BUTTONS
        // -------------------------------
        if (DrawUIButton(insertBtn)) {
            root = insertRec(root, inputValue);
            computeLayout(root,700,120,300);
        }

if (DrawUIButton(deleteBtn)) {
    if (selectedNode && !deleteAnimationActive) {
        deleteTargetNode = selectedNode;
        deleteAnimationActive = true;
        deleteTimer = 0.6f;
    } else if (!selectedNode) {
        // Search for the node to delete it with animation
        Node* toDelete = searchRecord(root, inputValue);
        if (toDelete) {
            deleteTargetNode = toDelete;
            deleteAnimationActive = true;
            deleteTimer = 0.6f;
        } else {
            // Node doesn't exist, just try to remove anyway (no-op)
            root = removeRec(root, inputValue);
            computeLayout(root,700,120,300);
        }
    }
}

        if (DrawUIButton(searchBtn)) {
            Node* res = searchRecord(root, inputValue);
            searchActive = true;
            searchTimer = 0;
            searchIndex = -1;
            searchFound = (res != nullptr);
        }

        // Visualize random BST
if (DrawUIButton(visualizeBtn)) {
    // === 1. Reset everything for a fresh demonstration ===
    // Delete old tree (if any)
    while (root) {
        root = removeRec(root, root->key);
    }

    // Reset all visualization state
    visualizeActive = true;
    visualizeSeq.clear();
    visualizeIndex = -1;
    visualizeTimer = 0.0f;

    // === 2. Generate 10 nice random distinct values ===
    for (int i = 0; i < 10; i++) {
        int value;
        do {
            value = GetRandomValue(1, 99);
        } while (std::find(visualizeSeq.begin(), visualizeSeq.end(), value) != visualizeSeq.end());
        visualizeSeq.push_back(value);
    }
}
        // -------------------------------
        // AUTO-VISUALIZE
        // -------------------------------
if (visualizeActive) {
    visualizeTimer += dt;
    if (visualizeTimer >= 0.6f && visualizeIndex < (int)visualizeSeq.size()-1) {
        visualizeTimer = 0;
        visualizeIndex++;
        root = insertRec(root, visualizeSeq[visualizeIndex]);
        computeLayout(root,700,120,300);
        
        // Stop visualization when done
        if (visualizeIndex >= (int)visualizeSeq.size()-1) {
            visualizeActive = false;
        }
    }
}

// -------------------------------
// SEARCH ANIMATION
// -------------------------------
if (searchActive) {
    searchTimer += dt;
    if (searchTimer >= 0.5f && searchIndex < (int)searchPath.size()-1) {
        searchTimer = 0;
        searchIndex++;
    }
    
    // Auto-reset search animation after completion
    if (searchIndex == (int)searchPath.size()-1 && searchTimer >= 1.5f) {
        searchActive = false;
        searchPath.clear();
        searchIndex = -1;
        searchTimer = 0.0f;
    }
}

        // -------------------------------
        // DELETE ANIMATION
        // -------------------------------
        if (deleteAnimationActive) {
            deleteTimer -= dt;
            if (deleteTimer <= 0) {
                int k = deleteTargetNode->key;
                root = removeRec(root, k);
                computeLayout(root,700,120,300);
                selectedNode = nullptr;
                deleteTargetNode = nullptr;
                deleteAnimationActive = false;
            }
        }

        // -------------------------------
        // NODE PICKING / DESELECT
        // -------------------------------
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {

            Vector2 mouseScreen = GetMousePosition();
            Vector2 mouseWorld  = ScreenToWorld(cam, mouseScreen);

            // ignore UI panel
            if (mouseScreen.x > 200) {
                Node* clicked = nullptr;
                pickNode(root, mouseWorld, 24, clicked);

                if (clicked) {
                    selectedNode = clicked;
                    searchActive = false;
                } else {
                    selectedNode = nullptr;
                }
            }
        }

        // Backspace delete selected node
        if (selectedNode && IsKeyPressed(KEY_BACKSPACE)) {
            deleteTargetNode = selectedNode;
            deleteAnimationActive = true;
            deleteTimer = 0.6f;
        }

        // =====================================================
        // RENDER TREE USING CAMERA (Mode2D)
        // =====================================================
        BeginMode2D(cam);

            updatePositions(root);
            drawEdges(root);
            drawNodes(root);

        EndMode2D();

        // =====================================================
        // NODE INFO PANEL
        // =====================================================
        if (selectedNode) {
            Node* p = findParent(root, selectedNode);

            Rectangle info = {1100, 40, 260, 160};
            DrawRectangleRec(info, Color{230,230,230,255});
            DrawRectangleLines(info.x, info.y, info.width, info.height, BLACK);

            DrawText("Node Info", info.x+10, info.y+10, 20, BLACK);
            DrawText(TextFormat("Key: %d", selectedNode->key), info.x+10, info.y+40, 18, BLACK);
            DrawText(TextFormat("Left: %s", selectedNode->left ? TextFormat("%d", selectedNode->left->key) : "null"),
                     info.x+10, info.y+60, 18, BLACK);
            DrawText(TextFormat("Right: %s", selectedNode->right ? TextFormat("%d", selectedNode->right->key) : "null"),
                     info.x+10, info.y+80, 18, BLACK);
            DrawText(TextFormat("Parent: %s", p ? TextFormat("%d", p->key) : "null"),
                     info.x+10, info.y+100, 18, BLACK);
            DrawText(TextFormat("Height: %d", nodeHeight(selectedNode)),
                     info.x+10, info.y+120, 18, BLACK);
            DrawText(TextFormat("Leaf: %s", isLeaf(selectedNode) ? "yes" : "no"),
                     info.x+10, info.y+140, 18, BLACK);
        }

        // END DRAW
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
