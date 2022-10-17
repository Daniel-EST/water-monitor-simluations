#include <random>
#include <stdlib.h>
#include <cstdio>
#include <raylib.h>
#include <fstream>

struct SimObject
{
    SimObject() = default;
    SimObject(int x, int y, int id);
    ~SimObject() = default;
    virtual void Update() = 0;
    virtual void Render() = 0;

    int x, y, id;
};

struct Node : SimObject
{
    Node(){}

    Node(int x, int y, int id, std::ofstream* data_file)
        : x{ x },
          y{ y },
          id{ id },
          data_file{ data_file }

    {
        left = nullptr;
        right = nullptr;
        active = true;
        alert = false;
        alert_old = false;
        warning = false;
        dragging = false;
        color = ColorAlpha(DARKBLUE, 0.5f);
        color_alert = GREEN;
    }

    ~Node(){}

    void Render() override
    {
        DrawCircle(x, y, 20, color);
        DrawCircle(x - 20, y - 20, 5, color_alert);
        DrawText(TextFormat("%d", id), x, y - 4, 18, RAYWHITE);
        
        if(left) DrawLine(x, y, left->x, left->y, BLACK);
        if(right) DrawLine(x, y, right->x, right->y, BLACK);

        Vector2 node_center{ (float) x, (float) y };
        Vector2 mouse_pos = GetMousePosition();
        if(CheckCollisionCircles(node_center, 20, mouse_pos, 1) || dragging)
        {
            if(IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            {
                DrawCircle(GetMouseX(), GetMouseY(), 20, ColorAlpha(color, 0.1));
                DrawCircle(GetMouseX() - 20, GetMouseY() - 20, 5, ColorAlpha(color_alert, 0.1));
            }
        }
    }

    void Update() override
    {
        move();
        failure();
    }

    void move() 
    {
        Vector2 node_center{ (float) x, (float) y };
        if(CheckCollisionCircles(node_center, 20.0f, GetMousePosition(), 1.0f) || dragging)
        {   
            if(IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            {
                dragging = true;
            }
            
            if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            {
                x = GetMouseX();
                y = GetMouseY();
                dragging = false;
            }
        }
    }

    void receive_water(bool contaminated, int step)
    {
        send_water(alert_old, step);
        if(contaminated) alert_children(contaminated);
        if(!contaminated) contaminated = generate_water_quality();
        color = contaminated ? RED : DARKBLUE;
        color = active ? color : ColorAlpha(DARKBLUE, 0.5f);
        alert_old = contaminated;

        int left_child = left ? left->id : 999;
        int right_child = right ? right->id : 999;

        (*data_file) <<  step << "," << id << "," << left_child << "," << right_child << "," << alert << "," << contaminated << "," << active << std::endl;
    }

    void alert_children(bool alert_received)
    {
        color_alert = alert_received ? YELLOW : GREEN;
        color_alert = active ? color_alert : BLANK;
        if(active)
        {
            if(left) left->alert_children(alert_received);
            if(right) right->alert_children(alert_received);
        }
    }

    void failure(float prob = 0.001f)
    {
        prob = active ? prob : 0.99; 
        std::random_device dev;
        std::uniform_real_distribution<double> runif(0.0, 1.0);
        active = runif(dev) < prob ? false : true;
    }
    
    void reset_alerts()
    {
        color_alert = GREEN;
    }

    void send_water(int step)
    {
        receive_water(  
            generate_water_quality(), step
        );
    }

    void send_water(bool contaminated, int step)
    {
        reset_alerts();
        if(left) left->receive_water(contaminated, step);
        if(right) right->receive_water(contaminated, step);
    }

    bool generate_water_quality(float prob = 0.01f)
    {
        std::random_device dev;
        std::uniform_real_distribution<double> runif(0.0, 1.0);
        
        if(runif(dev) < prob)
            return true;
            
        return false;
    }

    int x; 
    int y;
    int id;
    Node* left;
    Node* right;
    bool active;
    bool alert;
    bool alert_old;
    bool warning;
    bool dragging;
    Color color;
    Color color_alert;
    std::ofstream* data_file;
};

struct Master
{
    Master(std::ofstream* data_file) 
        : last_id{ 1 },
          data_file { data_file } {}

    void update_tree(Node* root)
    {
        root->Update();
        if(root->left) update_tree(root->left);
        if(root->right) update_tree(root->right);
    }

    void render_tree(Node* root)
    {
        root->Render();
        if(root->left) render_tree(root->left);
        if(root->right) render_tree(root->right);
    }

    Node* insert_node(Node* root, int parent_id)
    {
        Node* parent = search_by_id(root, parent_id);
        if(parent)
        {
            Node* new_node = new Node{ parent->x, parent->y + 75, ++last_id, data_file};
            if(parent->left)
            {
                new_node->x += 75;
                parent->right = parent->right ? parent->right : new_node;
            }
            else
            {
                new_node->x -= 75;
                parent->left = new_node;
            }

            return new_node;
        }
        return nullptr;
    }

    Node* search_by_id(Node* root, int id)
    {
        if(root->id == id) return root;
        Node* head = nullptr;
        if(root) head = root->left ? search_by_id(root->left, id) : nullptr;
        if(head) return head;
        if(root->right) head = root->right ? search_by_id(root->right, id) : nullptr;
        if(head) return head;
        
        return nullptr;
    }

    int last_id;
    std::ofstream* data_file;
};

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;


int main()
{   

    std::ofstream csv_file;
    csv_file.open("./data/data.csv");

    csv_file << "step,id,left_child,right_child,warning,contaminated,active" << std::endl;

    Master master{ &csv_file };
    Node n{ 400, 100, 1, &csv_file };

    char name[5 + 1] = "\0";
    int letterCount = 0;

    Rectangle textBox = { 400/2.0f - 100, 100, 100, 30 };
    bool mouseOnText = false;

    int framesCounter = 0;
    int new_id;

    int step{ 1 };

    int speed{ 60 };

    bool pause = false;

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "IoT Simulation");
    SetTargetFPS(60);

    while(!WindowShouldClose())
    {
        if (CheckCollisionPointRec(GetMousePosition(), textBox)) mouseOnText = true;
        else mouseOnText = false;

        if (mouseOnText)
        {
            SetMouseCursor(MOUSE_CURSOR_IBEAM);
            int key = GetCharPressed();
            while(key > 0)
            {
                if((key >= 48) && (key <= 57) && (letterCount < 5))
                {
                    name[letterCount] = (char) key;
                    name[letterCount + 1] = '\0';
                    letterCount++;
                }

                key = GetCharPressed(); 
            }

            if(IsKeyPressed(KEY_BACKSPACE))
            {
                letterCount--;
                if(letterCount < 0) letterCount = 0;
                name[letterCount] = '\0';
            }
        }
        else SetMouseCursor(MOUSE_CURSOR_DEFAULT);

        if(IsKeyPressed(KEY_ENTER))
        {
            new_id = std::atoi(name);
            master.insert_node(&n, new_id);
            letterCount = 0;
            name[letterCount] = '\0';
        }
        
        if(IsKeyPressed(KEY_P)) pause = !pause;
        {
            if((framesCounter % (speed * 1) == 0) || (IsKeyPressed(KEY_SPACE) && pause))
            {
                printf("Step %d\n", step++);
                n.send_water(step);
            }
        }

        if(IsKeyDown(KEY_KP_SUBTRACT))
            speed = (speed - 1) < 1 ? 1 : speed - 1;

        if(IsKeyDown(KEY_KP_ADD))
            speed++;

        master.update_tree(&n);
        
        BeginDrawing();
            ClearBackground(RAYWHITE);

            if(mouseOnText) DrawRectangleLines((int) textBox.x, (int) textBox.y, (int) textBox.width, (int) textBox.height, RED);
            else DrawRectangleLines((int) textBox.x, (int) textBox.y, (int) textBox.width, (int) textBox.height, DARKGRAY);

            DrawText(name, (int) textBox.x + 5, (int) textBox.y + 8, 18, MAROON);
            DrawText("Digite o ID do nó pai.", 75, 75, 16, DARKGRAY);
            DrawText(TextFormat("Speed %d", speed), 10, 10, 12, BLACK);

            if(mouseOnText)
            {
                if(letterCount < 5)
                {
                    if(((framesCounter / 20) % 2) == 0) DrawText("_", (int) textBox.x + 8 + MeasureText(name, 12), (int) textBox.y + 12, 16, MAROON);
                    DrawText("Aperte ENTER para criar um novo nó.", 30, 150, 14, GRAY);
                }
            }

            if(pause)
            {
                DrawText("Pause", 400 - (MeasureText("Pause", 30) / 2), 50, 30, BLACK);
                DrawText("Aperte SPACE para avançar", 400 - (MeasureText("Aperte SPACE para avançar", 20) / 2), 540, 20, BLACK);
            } 

            if(!pause) DrawText("Aperte P para pausar", 400 - (MeasureText("Aperte P para pausar", 20) / 2), 540, 20, BLACK);

            DrawText(TextFormat("Passo - %d", step), 14, 560, 18, BLACK);

            master.render_tree(&n);
        EndDrawing();

        if(!pause) framesCounter++;
    }

    csv_file.close();

    return 0;
}
