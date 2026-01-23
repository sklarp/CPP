#include <iostream>
#include <string>
#include <vector>

struct Pos
{
    int x{};
    int y{};

    friend bool operator==(const Pos& a, const Pos& b)
    {
        return a.x == b.x && a.y == b.y;
    }
};

void printMap(int size, const Pos& player, const Pos& exitPos, const std::vector<Pos>& obstacles)
{
    const std::string edge(static_cast<std::size_t>(size), '#');

    // Interior template: "#------#"
    std::string rowTemplate = edge;
    rowTemplate.replace(rowTemplate.begin() + 1, rowTemplate.end() - 1, std::string(static_cast<std::size_t>(size - 2), '-'));

    std::cout << edge << '\n';

    // Interior rows: 1 .. size-2
    for (int x = 1; x < size - 1; ++x)
    {
        std::string row = rowTemplate;

        // Place exit first, then player (player drawn "on top" if same cell)
        if (exitPos.x == x)
            row[static_cast<std::size_t>(exitPos.y)] = 'E';

        if (player.x == x)
            row[static_cast<std::size_t>(player.y)] = 'P';

        // Draw obstacles last (matches your original behavior)
        for (const auto& o : obstacles)
        {
            if (o.x == x)
                row[static_cast<std::size_t>(o.y)] = '^';
        }

        std::cout << row << '\n';
    }

    
    std::cout << edge << '\n';
}

Pos movePlayer(Pos player)
{
    char input{};
    if (!(std::cin >> input))
        return player; // input stream ended; keep position

    switch (input)
    {
    case 'w': --player.x; break;
    case 's': ++player.x; break;
    case 'a': --player.y; break;
    case 'd': ++player.y; break;
    default: break; // invalid input: do nothing
    }

    return player;
}

Pos clampToBounds(Pos player, int size)
{
    // Valid interior coordinates are:
    // x in [1, size-2], y in [1, size-2]
    const int min = 1;
    const int max = size - 2;

    if (player.x < min) player.x = min;
    if (player.x > max) player.x = max;
    if (player.y < min) player.y = min;
    if (player.y > max) player.y = max;

    return player;
}

bool isObstacle(const Pos& p, const std::vector<Pos>& obstacles)
{
    for (const auto& o : obstacles)
    {
        if (o == p)
            return true;
    }
    return false;
}

void gameLoop()
{
    const int size{8}; // adjust maze size here

    Pos player{1, 1};
    const Pos exitPos{size - 2, size - 2}; // bottom-right interior cell

    const std::vector<Pos> obstacles{
        {2, 3}, {2, 2}, {3, 1}, {4, 4}, {4, 3}
    };

    bool done{false};

    while (!done)
    {
        printMap(size, player, exitPos, obstacles);

        player = movePlayer(player);
        player = clampToBounds(player, size);

        if (isObstacle(player, obstacles))
        {
            printMap(size, player, exitPos, obstacles);
            std::cout << "You lose !";
            done = true;
        }
        else if (player == exitPos)
        {
            printMap(size, player, exitPos, obstacles);
            std::cout << "You Win !";
            done = true;
        }
    }
}

int main()
{
    gameLoop();
    return 0;
}
