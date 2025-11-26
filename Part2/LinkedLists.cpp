#include "raylib.h"
#include <iostream>
#include <string>
#include <cmath>

// ==========================================================
// WINDOW SETTINGS
// ==========================================================
const int SCREEN_WIDTH  = 1280;
const int SCREEN_HEIGHT = 720;

// NODE VISUAL SETTINGS
const float NODE_WIDTH   = 110.0f;
const float NODE_HEIGHT  = 45.0f;
const float NODE_SPACING = 150.0f;
const float NODE_Y       = 260.0f;

// DATA/NEXT proportion
const float DATA_PORTION = 0.6f;

// ==========================================================
// HELPER: LERP
// ==========================================================
float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// ==========================================================
// NODE
// ==========================================================
struct Node {
    int value;
    Node* next;

    float x, y;
    float targetX, targetY;

    bool highlighted;

    Node(int v)
        : value(v), next(nullptr),
          x(0), y(0), targetX(0), targetY(0),
          highlighted(false) {}
};

// ==========================================================
// LINKED LIST
// ==========================================================
class LinkedList {
public:
    LinkedList();
    ~LinkedList();

    void InsertHead(int value);
    void InsertTail(int value);

    void DeleteHead();
    void DeleteTail();
    void DeleteByPointer(Node* node);

    void MoveNodeBefore(Node* node, Node* target);
    void MoveNodeAfter(Node* node, Node* target);

    void UpdateLayout();
    void UpdateAnimation(float dt, Node* dragging);
    void Draw(Node* selected, float pulse,
              Node* dropTarget, bool isDragging, bool dropBefore) const;

    Node* GetHead() const { return head; }

private:
    Node* head;
    int   count;

    void FreeList();
};

// ==========================================================
// UI BUTTON
// ==========================================================
struct UIButton {
    Rectangle rect;
    std::string label;
    Color baseColor;
};

bool IsButtonClicked(const UIButton& btn) {
    return CheckCollisionPointRec(GetMousePosition(), btn.rect) &&
           IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

void DrawButton(const UIButton& btn) {
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, btn.rect);
    bool clicked = hover && IsMouseButtonDown(MOUSE_LEFT_BUTTON);

    Color c = btn.baseColor;
    if (hover) {
        c.r = (unsigned char)(c.r * 0.90f);
        c.g = (unsigned char)(c.g * 0.90f);
        c.b = (unsigned char)(c.b * 0.90f);
    }

    Rectangle drawRect = btn.rect;
    if (clicked) {
        drawRect.y += 2;
        drawRect.height -= 4;
    }

    DrawRectangleRec(drawRect, c);
    DrawRectangleLinesEx(drawRect, 2, BLACK);

    int font = 18;
    int w = MeasureText(btn.label.c_str(), font);
    DrawText(btn.label.c_str(),
             (int)(drawRect.x + drawRect.width/2 - w/2),
             (int)(drawRect.y + drawRect.height/2 - font/2),
             font, BLACK);
}

// ==========================================================
// ARROW DRAW
// ==========================================================
void DrawArrow(Vector2 start, Vector2 end, float thickness, Color color) {
    DrawLineEx(start, end, thickness, color);

    Vector2 dir = { end.x - start.x, end.y - start.y };
    float len = sqrtf(dir.x*dir.x + dir.y*dir.y);
    if (len <= 0.01f) return;

    dir.x /= len;
    dir.y /= len;

    Vector2 left  = { end.x - dir.x*12 - dir.y*6,  end.y - dir.y*12 + dir.x*6 };
    Vector2 right = { end.x - dir.x*12 + dir.y*6,  end.y - dir.y*12 - dir.x*6 };

    DrawTriangle(end, left, right, color);
}

// ==========================================================
// LINKED LIST IMPLEMENTATION
// ==========================================================
LinkedList::LinkedList() : head(nullptr), count(0) {}
LinkedList::~LinkedList() { FreeList(); }

void LinkedList::FreeList() {
    Node* t = head;
    while (t) {
        Node* n = t->next;
        delete t;
        t = n;
    }
    head = nullptr;
    count = 0;
}

void LinkedList::InsertHead(int value) {
    Node* n = new Node(value);
    n->next = head;
    head = n;
    count++;
    UpdateLayout();
    n->x = n->targetX - 150;
    n->y = n->targetY;
}

void LinkedList::InsertTail(int value) {
    Node* n = new Node(value);
    if (!head) {
        head = n;
        count++;
        UpdateLayout();
        n->x = n->targetX + 150;
        n->y = n->targetY;
        return;
    }
    Node* t = head;
    while (t->next) t = t->next;
    t->next = n;
    count++;
    UpdateLayout();
    n->x = n->targetX + 150;
    n->y = n->targetY;
}

void LinkedList::DeleteHead() {
    if (!head) return;
    Node* old = head;
    head = head->next;
    delete old;
    count--;
    UpdateLayout();
}

void LinkedList::DeleteTail() {
    if (!head) return;
    if (!head->next) {
        delete head;
        head = nullptr;
        count = 0;
        return;
    }
    Node* t = head;
    while (t->next->next) t = t->next;
    delete t->next;
    t->next = nullptr;
    count--;
    UpdateLayout();
}

void LinkedList::DeleteByPointer(Node* node) {
    if (!node || !head) return;
    if (node == head) { DeleteHead(); return; }

    Node* prev = head;
    while (prev && prev->next != node) prev = prev->next;
    if (!prev) return;

    prev->next = node->next;
    delete node;
    count--;
    UpdateLayout();
}

void LinkedList::MoveNodeBefore(Node* node, Node* target) {
    if (!node || !target || node == target || !head) return;
    if (node->next == target) return;

    Node* prevNode = nullptr;
    Node* prevTarget = nullptr;
    Node* cur = head;

    while (cur) {
        if (cur->next == node) prevNode = cur;
        if (cur->next == target) prevTarget = cur;
        cur = cur->next;
    }

    if (node == head) {
        head = node->next;
    } else if (prevNode) {
        prevNode->next = node->next;
    } else {
        return;
    }

    node->next = target;
    if (target == head) {
        head = node;
    } else if (prevTarget) {
        prevTarget->next = node;
    }

    UpdateLayout();
}

void LinkedList::MoveNodeAfter(Node* node, Node* target) {
    if (!node || !target || node == target || !head) return;
    if (target->next == node) return;

    Node* prevNode = nullptr;
    Node* cur = head;
    while (cur && cur->next != node) {
        cur = cur->next;
    }
    if (cur) prevNode = cur;

    if (node == head) {
        head = node->next;
    } else if (prevNode) {
        prevNode->next = node->next;
    } else {
        return;
    }

    node->next = target->next;
    target->next = node;

    UpdateLayout();
}

void LinkedList::UpdateLayout() {
    float x = 80.0f;
    Node* t = head;
    while (t) {
        t->targetX = x;
        t->targetY = NODE_Y;
        x += NODE_SPACING;
        t = t->next;
    }
}

void LinkedList::UpdateAnimation(float dt, Node* dragging) {
    const float speed = 10.0f;
    Node* t = head;
    while (t) {
        if (t != dragging) {
            t->x = Lerp(t->x, t->targetX, speed * dt);
            t->y = Lerp(t->y, t->targetY, speed * dt);
        }
        t = t->next;
    }
}

void LinkedList::Draw(Node* selected, float pulse,
                      Node* dropTarget, bool isDragging, bool dropBefore) const {
    Node* t = head;
    const float dataW = NODE_WIDTH * DATA_PORTION;
    const float nextW = NODE_WIDTH - dataW;

    while (t) {
        bool isSel = (t == selected);
        Color fill = LIGHTGRAY;
        if (t->highlighted) fill = YELLOW;
        if (isSel) {
            float r = Lerp((float)LIGHTGRAY.r, (float)RED.r, pulse);
            float g = Lerp((float)LIGHTGRAY.g, (float)RED.g, pulse);
            float b = Lerp((float)LIGHTGRAY.b, (float)RED.b, pulse);
            fill = { (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
        }

        DrawRectangle((int)t->x, (int)t->y, (int)NODE_WIDTH, (int)NODE_HEIGHT, fill);
        DrawRectangleLines((int)t->x, (int)t->y, (int)NODE_WIDTH, (int)NODE_HEIGHT, BLACK);
        DrawLine((int)(t->x + dataW), (int)t->y, (int)(t->x + dataW), (int)(t->y + NODE_HEIGHT), BLACK);

        int labelFont = 14;
        float labelY = t->y - 20.0f;
        DrawText("DATA", (int)(t->x + dataW/2 - MeasureText("DATA", labelFont)/2), (int)labelY, labelFont, BLACK);
        DrawText("NEXT", (int)(t->x + dataW + nextW/2 - MeasureText("NEXT", labelFont)/2), (int)labelY, labelFont, BLACK);

        if (t == head) {
            float dataCenterX = t->x + dataW / 2.0f;
            float headY = t->y - 70.0f;
            DrawText("HEAD", (int)(dataCenterX - MeasureText("HEAD", 20)/2), (int)headY, 20, BLACK);
            DrawArrow({dataCenterX, headY + 24}, {dataCenterX, t->y - 14}, 2.0f, BLACK);
        }

        std::string valStr = std::to_string(t->value);
        DrawText(valStr.c_str(), (int)(t->x + dataW/2 - MeasureText(valStr.c_str(), 18)/2),
                 (int)(t->y + NODE_HEIGHT/2 - 9), 18, BLACK);

        std::string nextStr = t->next ? "->" : "-";
        DrawText(nextStr.c_str(), (int)(t->x + dataW + nextW/2 - MeasureText(nextStr.c_str(), 18)/2),
                 (int)(t->y + NODE_HEIGHT/2 - 9), 18, BLACK);

        if (t->next) {
            Vector2 s = { t->x + NODE_WIDTH, t->y + NODE_HEIGHT/2 };
            Vector2 e = { t->next->x, t->next->y + NODE_HEIGHT/2 };
            DrawArrow(s, e, 3.0f, DARKGRAY);
        }

        if (isDragging && dropTarget == t) {
            Color indColor = dropBefore ? GREEN : RED;
            float indX = dropBefore ? t->x - 6 : t->x + NODE_WIDTH + 3;
            DrawRectangle((int)indX, (int)t->y, 6, (int)NODE_HEIGHT, indColor);
        }

        t = t->next;
    }

    if (head) {
        Node* last = head;
        while (last->next) last = last->next;
        Vector2 s = { last->x + NODE_WIDTH, last->y + NODE_HEIGHT/2 };
        Vector2 e = { last->x + NODE_WIDTH + 60, last->y + NODE_HEIGHT/2 };
        DrawArrow(s, e, 3, DARKGRAY);
        DrawText("NULL", (int)e.x + 10, (int)e.y - 10, 20, DARKGRAY);
    }
}

// ==========================================================
// MAIN
// ==========================================================
int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Linked List Visualiser - Drag & Drop + Dummy Node");
    SetTargetFPS(60);

    LinkedList list;

    UIButton btnInsertHead = { {50, 500, 150, 40}, "Insert Head", GREEN };
    UIButton btnInsertTail = { {220, 500, 150, 40}, "Insert Tail", BLUE };
    UIButton btnDeleteHead = { {390, 500, 150, 40}, "Delete Head", RED };
    UIButton btnDeleteTail = { {560, 500, 150, 40}, "Delete Tail", ORANGE };
    UIButton btnTraverse   = { {730, 500, 150, 40}, "Traverse", PURPLE };
    UIButton btnAddDummy   = { {900, 500, 150, 40}, "Add Dummy", MAROON };  // NEW BUTTON

    std::string inputBuffer;
    std::string status = "Enter a number and use buttons or drag nodes to reorder.";

    Node* selectedNode = nullptr;
    float flashTime = 0.0f;

    Node* draggingNode = nullptr;
    bool isDragging = false;
    float dragOffsetX = 0, dragOffsetY = 0;
    Node* dropTarget = nullptr;
    bool dropBefore = false;

    bool traversing = false;
    Node* travNode = nullptr;
    float travTimer = 0.0f;

    auto ClearTraversal = [&]() {
        traversing = false;
        travNode = nullptr;
        travTimer = 0.0f;
        Node* t = list.GetHead();
        while (t) { t->highlighted = false; t = t->next; }
    };

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        flashTime += dt;
        float pulse = (sinf(flashTime * 4.0f) + 1.0f) * 0.5f;

        // Input
        int c = GetCharPressed();
        while (c > 0) {
            if (c >= '0' && c <= '9') inputBuffer += (char)c;
            c = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (!inputBuffer.empty()) inputBuffer.pop_back();
            else if (selectedNode && !isDragging) {
                ClearTraversal();
                list.DeleteByPointer(selectedNode);
                selectedNode = nullptr;
                status = "Deleted selected node.";
            }
        }

        // Mouse down: start drag or select
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mouse = GetMousePosition();
            Node* clicked = nullptr;
            Node* t = list.GetHead();
            while (t) {
                if (CheckCollisionPointRec(mouse, {t->x, t->y, NODE_WIDTH, NODE_HEIGHT})) {
                    clicked = t;
                    break;
                }
                t = t->next;
            }

            if (clicked) {
                selectedNode = clicked;
                draggingNode = clicked;
                isDragging = true;
                dragOffsetX = mouse.x - clicked->x;
                dragOffsetY = mouse.y - clicked->y;
                flashTime = 0.0f;
            }
        }

        // Drag update
        dropTarget = nullptr;
        dropBefore = false;

        if (isDragging && draggingNode && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            Vector2 mouse = GetMousePosition();
            draggingNode->x = mouse.x - dragOffsetX;
            draggingNode->y = mouse.y - dragOffsetY;

            float mouseX = mouse.x;
            Node* bestTarget = nullptr;
            bool bestIsBefore = true;
            float bestDist = 1e9;

            Node* t = list.GetHead();
            while (t) {
                if (t == draggingNode) { t = t->next; continue; }

                float centerX = t->x + NODE_WIDTH / 2;
                float dist = fabsf(mouseX - centerX);

                if (dist < bestDist) {
                    bestDist = dist;
                    bestTarget = t;
                    bestIsBefore = (mouseX < centerX);
                }
                t = t->next;
            }

            dropTarget = bestTarget;
            dropBefore = bestIsBefore;
        }

        // Drop
        if (isDragging && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            if (draggingNode && dropTarget && draggingNode != dropTarget) {
                ClearTraversal();
                if (dropBefore) {
                    list.MoveNodeBefore(draggingNode, dropTarget);
                    status = "Moved node before target.";
                } else {
                    list.MoveNodeAfter(draggingNode, dropTarget);
                    status = "Moved node after target.";
                }
            }
            isDragging = false;
            draggingNode = nullptr;
            dropTarget = nullptr;
        }

        // Buttons
        if (IsButtonClicked(btnInsertHead) && !inputBuffer.empty()) {
            ClearTraversal();
            list.InsertHead(std::stoi(inputBuffer));
            inputBuffer.clear();
            status = "Inserted at head.";
        }
        if (IsButtonClicked(btnInsertTail) && !inputBuffer.empty()) {
            ClearTraversal();
            list.InsertTail(std::stoi(inputBuffer));
            inputBuffer.clear();
            status = "Inserted at tail.";
        }
        if (IsButtonClicked(btnDeleteHead)) { ClearTraversal(); list.DeleteHead(); status = "Deleted head."; }
        if (IsButtonClicked(btnDeleteTail)) { ClearTraversal(); list.DeleteTail(); status = "Deleted tail."; }
        if (IsButtonClicked(btnTraverse)) {
            ClearTraversal();
            travNode = list.GetHead();
            traversing = travNode != nullptr;
            if (travNode) travNode->highlighted = true;
            status = "Traversing...";
        }
        if (IsButtonClicked(btnAddDummy)) {
            ClearTraversal();
            list.InsertTail(999);  // Dummy node value
            status = "Added dummy node (999) at tail.";
        }

        // Traversal animation
        if (traversing) {
            travTimer += dt;
            if (travTimer > 0.5f) {
                if (travNode) travNode->highlighted = false;
                travNode = travNode ? travNode->next : nullptr;
                if (travNode) travNode->highlighted = true;
                else traversing = false;
                travTimer = 0.0f;
            }
        }

        list.UpdateAnimation(dt, draggingNode);

        // Draw
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Linked List Visualizer", 40, 20, 32, DARKBLUE);
        DrawText("Backspace on selected node to delete", 40, 70, 20, DARKGRAY);

        // Input box
        DrawText("Value:", 50, 320, 20, BLACK);
        DrawRectangle(50, 350, 200, 40, LIGHTGRAY);
        DrawRectangleLines(50, 350, 200, 40, BLACK);
        DrawText(inputBuffer.empty() ? "0" : inputBuffer.c_str(), 60, 360, 20, inputBuffer.empty() ? GRAY : BLACK);

        DrawButton(btnInsertHead);
        DrawButton(btnInsertTail);
        DrawButton(btnDeleteHead);
        DrawButton(btnDeleteTail);
        DrawButton(btnTraverse);
        DrawButton(btnAddDummy);  // Draw the new button

        DrawText(status.c_str(), 50, 450, 18, DARKGRAY);

        list.Draw(selectedNode, pulse, dropTarget, isDragging, dropBefore);

        // Info panel
        if (selectedNode) {
            DrawRectangle(850, 320, 380, 160, Fade(LIGHTGRAY, 0.9f));
            DrawRectangleLines(850, 320, 380, 160, BLACK);
            DrawText("Selected Node", 870, 340, 24, BLACK);
            DrawText(("Value: " + std::to_string(selectedNode->value)).c_str(), 870, 380, 20, BLACK);
            std::string nextStr = selectedNode->next ? ("Next: " + std::to_string(selectedNode->next->value)) : "Next: NULL";
            DrawText(nextStr.c_str(), 870, 410, 20, BLACK);

            Node* prev = nullptr;
            Node* cur = list.GetHead();
            while (cur && cur->next != selectedNode) { prev = cur; cur = cur->next; }
            if (prev || selectedNode == list.GetHead()) {
                std::string prevMsg;
                if (prev) prevMsg = "Prev: " + std::to_string(prev->value);
                else prevMsg = "Prev: NULL";
                DrawText(prevMsg.c_str(), 870, 440, 20, BLACK);
            }
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
