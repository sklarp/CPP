#include <iostream>
#include <string>
#include <tuple>
#include <vector>

static bool win{0};

void PrintMap(int height, std::tuple<int, int> player, std::tuple<int, int> exit, std::vector<std::tuple<int, int>> obstacles)
{
    int Px {std::get<0>(player)};
    int Py {std::get<1>(player)};

    int Ex {std::get<0>(exit)};
    int Ey {std::get<1>(exit)};

    
    std::string edge(height, '#');
    std::string row_template = edge;
    
    row_template.replace(row_template.begin()+1, row_template.end()-1, std::string (height-2, '-'));

    std::cout << edge << '\n';
    for (int i = 1; i <= height-1; i++)
    {
        std::string row = row_template;
        if (Ex == i)
        {
            row.replace(Ey, 1, "E");
        }
        if (Px == i)
        {
            row.replace(Py, 1, "P");
        }
        for(auto [x,y] : obstacles)
        {
            if (x == i)
            {
                row.replace(y, 1, "^");
            }
        }
        std::cout << row << '\n';
        
    }
    std::cout << edge << '\n';
}

std::tuple<int, int> MovePlayer(std::tuple<int, int> player)
{
    char input;
    std::cin >> input;
    
    switch(input)
    {
        case 'w':
            std::get<0>(player)--;
            break;
        case 's':
            std::get<0>(player)++;
            break;
        case 'a':
            std::get<1>(player)--;
            break;
        case 'd':
            std::get<1>(player)++;
            break;
        default:
            break;
    }
    return player;
}

std::tuple<int, int> CheckBounds(std::tuple<int, int> player, int height)
{
    if (std::get<0>(player) > height-1){
            std::get<0>(player)--;
    }
    if (std::get<1>(player) > height-2){
        std::get<1>(player)--;
    }
    if (std::get<0>(player) < 1){
        std::get<0>(player)++;
    }
    if (std::get<1>(player) < 1){
        std::get<1>(player)++;
    }
    return player;
}



void Gameloop(bool win)
{
    static int height {8};

    std::tuple<int, int> player {1,1};
    std::tuple<int, int> exit {height-1,height-2};
    std::vector<std::tuple<int, int>> obstacles {{2,3}, {2,2}, {3,1}, {4, 4}, {4, 3}};

    while(!win)
    {
        PrintMap(height, player, exit, obstacles);

        

        player = MovePlayer(player);
        player = CheckBounds(player, height);

        int Px = std::get<0>(player);
        int Py = std::get<1>(player);

        int Ex = std::get<0>(exit);
        int Ey = std::get<1>(exit);
        
        for(auto [x,y] : obstacles)
        {
            if (x == Px && y == Py)
            {
                PrintMap(height, player, exit, obstacles);
                std::cout << "You lose !";
                win = true;
            }
        }

        if (Px == Ex && Py == Ey)
        {
            PrintMap(height, player, exit, obstacles);
            std::cout << "You Win !";
            win = true;
        }
        
    }
}

int main()
{
    Gameloop(win);
    return 0;
}