#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <stdexcept>
#include <random>
#include <algorithm>
#include <cstdlib>

// Позиция клетки

struct Position {
    int row;
    int col;

    bool operator==(const Position& other) const {
        return row == other.row && col == other.col;
    }

    bool operator!=(const Position& other) const {
        return !(*this == other);
    }
};

std::ostream& operator<<(std::ostream& out, const Position& p) {
    out << "(" << p.row << ", " << p.col << ")";
    return out;
}

//  Структура лабиринта 

// T - данные, которые лежат в каждой клетке
// Лабиринт сам владеет клетками
// указателей нет, поэтому удалять вручную ничего не нужно.
template <typename T>
class Maze {
public:
    Maze(int rows, int cols, const T& initial = T()) : _rows(rows), _cols(cols), _passageCount(0) {
        if (rows < 2 || cols < 2) {
            throw std::invalid_argument("лабиринт должен быть минимум 2x2");
        }
        _data.assign(rows * cols, initial);
        _rightOpen.assign(rows * cols, false);
        _downOpen.assign(rows * cols, false);
    }

    int rows() const { return _rows; }
    int cols() const { return _cols; }
    int cellCount() const { return _rows * _cols; }
    int passageCount() const { return _passageCount; }

    bool contains(Position p) const {
        return p.row >= 0 && p.row < _rows && p.col >= 0 && p.col < _cols;
    }

    // номер клетки в одномерном массиве
    int indexOf(Position p) const {
        checkInside(p);
        return p.row * _cols + p.col;
    }

    // соседи по сторонам (стены не учитываются)
    std::vector<Position> neighbors(Position p) const {
        checkInside(p);
        std::vector<Position> result;
        Position around[4] = {
            {p.row - 1, p.col},   // сверху
            {p.row + 1, p.col},   // снизу
            {p.row, p.col - 1},   // слева
            {p.row, p.col + 1}    // справа
        };
        for (int i = 0; i < 4; i++) {
            if (contains(around[i])) {
                result.push_back(around[i]);
            }
        }
        return result;
    }

    bool areAdjacent(Position a, Position b) const {
        int dr = std::abs(a.row - b.row);
        int dc = std::abs(a.col - b.col);
        return dr + dc == 1;
    }

    // сделать проход между соседними клетками
    void connect(Position a, Position b) {
        checkPair(a, b);
        if (hasPassage(a, b)) {
            return;
        }
        Position first = topLeft(a, b);
        if (a.row == b.row) {
            _rightOpen[indexOf(first)] = true;
        } else {
            _downOpen[indexOf(first)] = true;
        }
        _passageCount++;
    }

    bool hasPassage(Position a, Position b) const {
        checkPair(a, b);
        Position first = topLeft(a, b);
        if (a.row == b.row) {
            return _rightOpen[indexOf(first)];
        }
        return _downOpen[indexOf(first)];
    }

    const T& getData(Position p) const {
        return _data[indexOf(p)];
    }

    void setData(Position p, const T& value) {
        _data[indexOf(p)] = value;
    }

private:
    int _rows;
    int _cols;
    int _passageCount;
    std::vector<T> _data;
    // у каждой клетки храним только проход вправо и вниз,
    // проход влево/вверх - это проход вправо/вниз у соседа
    std::vector<bool> _rightOpen;
    std::vector<bool> _downOpen;

    void checkInside(Position p) const {
        if (!contains(p)) {
            throw std::out_of_range("клетка вне лабиринта");
        }
    }

    void checkPair(Position a, Position b) const {
        checkInside(a);
        checkInside(b);
        if (!areAdjacent(a, b)) {
            throw std::invalid_argument("проход возможен только между соседними клетками");
        }
    }

    // из двух соседних клеток та, что левее или выше
    Position topLeft(Position a, Position b) const {
        if (a.row < b.row || a.col < b.col) {
            return a;
        }
        return b;
    }
};

// Правила для алгоритмов

// порядок случ при генерации соседей
class RandomOrder {
public:
    RandomOrder(unsigned seed) : _rng(seed) {}

    void operator()(std::vector<Position>& list) {
        std::shuffle(list.begin(), list.end(), _rng);
    }

private:
    std::mt19937 _rng;
};

// порядок соседей при генерации
class FixedOrder {
public:
    void operator()(std::vector<Position>&) {}
};

// можно идти, если между клетками есть проход
template <typename T>
class ThroughPassages {
public:
    ThroughPassages(const Maze<T>& maze) : _maze(maze) {}

    bool operator()(Position from, Position to) const {
        return _maze.hasPassage(from, to);
    }

private:
    const Maze<T>& _maze;
};

// условия проходки
class AvoidTraps {
public:
    AvoidTraps(const Maze<char>& maze) : _maze(maze) {}

    bool operator()(Position from, Position to) const {
        return _maze.hasPassage(from, to) && _maze.getData(to) != 'X';
    }

private:
    const Maze<char>& _maze;
};

//  Алгоритмы 

// генерация поиском в глубину с возвратом
template <typename T, typename Order>
void generateMaze(Maze<T>& maze, Position start, Order order) {
    if (maze.passageCount() != 0) {
        throw std::logic_error("генерировать можно только лабиринт без проходов");
    }

    std::vector<bool> visited(maze.cellCount(), false);
    std::vector<Position> stack;

    visited[maze.indexOf(start)] = true;
    stack.push_back(start);

    while (!stack.empty()) {
        Position current = stack.back();

        // соседи, где ещё не были
        std::vector<Position> notVisited;
        std::vector<Position> near = maze.neighbors(current);
        for (size_t i = 0; i < near.size(); i++) {
            if (!visited[maze.indexOf(near[i])]) {
                notVisited.push_back(near[i]);
            }
        }

        if (notVisited.empty()) {
            stack.pop_back();
            continue;
        }

        order(notVisited);
        Position next = notVisited[0];

        maze.connect(current, next);   // ломаем стену
        visited[maze.indexOf(next)] = true;
        stack.push_back(next);
    }
}

// восстановление пути
template <typename T>
std::vector<Position> restorePath(const Maze<T>& maze, const std::vector<Position>& cameFrom,
                                  Position start, Position finish) {
    std::vector<Position> path;
    Position current = finish;
    path.push_back(current);
    while (current != start) {
        current = cameFrom[maze.indexOf(current)];
        path.push_back(current);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

// поиск кратчайшего пути поиском в ширину
// canMove решает, можно ли перейти из одной клетки в соседнюю
template <typename T, typename CanMove>
std::vector<Position> findPath(const Maze<T>& maze, Position start, Position finish, CanMove canMove) {
    if (!maze.contains(start) || !maze.contains(finish)) {
        throw std::out_of_range("клетка вне лабиринта");
    }

    std::vector<bool> visited(maze.cellCount(), false);
    std::vector<Position> cameFrom(maze.cellCount(), start);
    std::queue<Position> queue;

    visited[maze.indexOf(start)] = true;
    queue.push(start);

    while (!queue.empty()) {
        Position current = queue.front();
        queue.pop();

        if (current == finish) {
            return restorePath(maze, cameFrom, start, finish);
        }

        std::vector<Position> near = maze.neighbors(current);
        for (size_t i = 0; i < near.size(); i++) {
            Position next = near[i];
            if (!visited[maze.indexOf(next)] && canMove(current, next)) {
                visited[maze.indexOf(next)] = true;
                cameFrom[maze.indexOf(next)] = current;
                queue.push(next);
            }
        }
    }

    return std::vector<Position>();
}

// из клетки (0,0) можно дойти до любой
template <typename T>
bool isConnected(const Maze<T>& maze) {
    Position start = {0, 0};
    for (int r = 0; r < maze.rows(); r++) {
        for (int c = 0; c < maze.cols(); c++) {
            Position target = {r, c};
            if (findPath(maze, start, target, ThroughPassages<T>(maze)).empty()) {
                return false;
            }
        }
    }
    return true;
}

// 
template <typename T>
bool isPerfect(const Maze<T>& maze) {
    return isConnected(maze) && maze.passageCount() == maze.cellCount() - 1;
}

//  Вывод 

bool inPath(const std::vector<Position>& path, Position p) {
    for (size_t i = 0; i < path.size(); i++) {
        if (path[i] == p) {
            return true;
        }
    }
    return false;
}

// S - старт, F - финиш, * - маршрут
template <typename T>
void printMaze(std::ostream& out, const Maze<T>& maze, Position start, Position finish,
               const std::vector<Position>& path) {
    out << "+";
    for (int c = 0; c < maze.cols(); c++) {
        out << "---+";
    }
    out << std::endl;

    for (int r = 0; r < maze.rows(); r++) {
        // строка с клетками и вертикальными стенами
        out << "|";
        for (int c = 0; c < maze.cols(); c++) {
            Position p = {r, c};
            char mark = ' ';
            if (p == start) {
                mark = 'S';
            } else if (p == finish) {
                mark = 'F';
            } else if (inPath(path, p)) {
                mark = '*';
            }
            out << " " << mark << " ";

            bool open = c + 1 < maze.cols() && maze.hasPassage(p, Position{r, c + 1});
            out << (open ? ' ' : '|');
        }
        out << std::endl;

        // строка с горизонтальными стенами под клетками
        out << "+";
        for (int c = 0; c < maze.cols(); c++) {
            bool open = r + 1 < maze.rows() && maze.hasPassage(Position{r, c}, Position{r + 1, c});
            out << (open ? "   " : "---") << "+";
        }
        out << std::endl;
    }
}

void printPath(std::ostream& out, const std::vector<Position>& path) {
    if (path.empty()) {
        out << "пути нет" << std::endl;
        return;
    }
    for (size_t i = 0; i < path.size(); i++) {
        if (i > 0) {
            out << " -> ";
        }
        out << path[i];
    }
    out << std::endl;
}

//  Тесты 

class Tester {
public:
    void check(bool condition, const std::string& name) {
        if (condition) {
            _passed++;
            std::cout << "  [OK]   " << name << std::endl;
        } else {
            _failed++;
            std::cout << "  [FAIL] " << name << std::endl;
        }
    }

    void report() const {
        std::cout << std::endl << "Пройдено: " << _passed << ", провалено: " << _failed << std::endl;
    }

    bool allPassed() const {
        return _failed == 0;
    }

private:
    int _passed = 0;
    int _failed = 0;
};

// проверка, что путь правильный: от start до finish и каждый шаг через проход
template <typename T>
bool isValidPath(const Maze<T>& maze, const std::vector<Position>& path, Position start, Position finish) {
    if (path.empty() || path.front() != start || path.back() != finish) {
        return false;
    }
    for (size_t i = 0; i + 1 < path.size(); i++) {
        if (!maze.areAdjacent(path[i], path[i + 1]) || !maze.hasPassage(path[i], path[i + 1])) {
            return false;
        }
    }
    return true;
}

// открыть вообще все проходы (для тестов)
template <typename T>
void openAll(Maze<T>& maze) {
    for (int r = 0; r < maze.rows(); r++) {
        for (int c = 0; c < maze.cols(); c++) {
            if (c + 1 < maze.cols()) maze.connect(Position{r, c}, Position{r, c + 1});
            if (r + 1 < maze.rows()) maze.connect(Position{r, c}, Position{r + 1, c});
        }
    }
}

void testCreate(Tester& t) {
    std::cout << "Создание лабиринта:" << std::endl;

    int badSizes[4][2] = { {1, 5}, {5, 1}, {0, 0}, {-3, 3} };
    for (int i = 0; i < 4; i++) {
        bool thrown = false;
        try {
            Maze<int> bad(badSizes[i][0], badSizes[i][1]);
        } catch (const std::invalid_argument&) {
            thrown = true;
        }
        t.check(thrown, "размер " + std::to_string(badSizes[i][0]) + "x" +
                        std::to_string(badSizes[i][1]) + " запрещён");
    }

    Maze<int> m(2, 3, 7);
    t.check(m.rows() == 2 && m.cols() == 3 && m.cellCount() == 6, "размер 2x3 создаётся");
    t.check(m.passageCount() == 0, "в новом лабиринте нет проходов");
    t.check(m.getData(Position{1, 2}) == 7, "клетки заполнены начальным значением");

    m.setData(Position{0, 1}, 42);
    t.check(m.getData(Position{0, 1}) == 42, "данные клетки меняются");

    t.check(m.contains(Position{1, 2}) && !m.contains(Position{2, 0}) && !m.contains(Position{0, -1}),
            "contains проверяет границы");

    bool thrown = false;
    try {
        m.getData(Position{5, 5});
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    t.check(thrown, "данные за границей получить нельзя");
}

void testNeighbors(Tester& t) {
    std::cout << "Соседи:" << std::endl;
    Maze<int> m(3, 3);

    t.check(m.neighbors(Position{0, 0}).size() == 2, "у угла 2 соседа");
    t.check(m.neighbors(Position{0, 1}).size() == 3, "у края 3 соседа");
    t.check(m.neighbors(Position{1, 1}).size() == 4, "в центре 4 соседа");
    t.check(m.areAdjacent(Position{0, 0}, Position{0, 1}), "клетки рядом соседние");
    t.check(!m.areAdjacent(Position{0, 0}, Position{1, 1}), "по диагонали не соседи");
    t.check(!m.areAdjacent(Position{0, 0}, Position{0, 0}), "клетка не сосед сама себе");

    bool thrown = false;
    try {
        m.neighbors(Position{3, 0});
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    t.check(thrown, "соседи клетки за границей - ошибка");
}

void testPassages(Tester& t) {
    std::cout << "Проходы:" << std::endl;
    Maze<int> m(3, 3);
    Position a = {1, 1};
    Position b = {1, 2};

    t.check(!m.hasPassage(a, b), "сначала прохода нет");
    m.connect(a, b);
    t.check(m.hasPassage(a, b) && m.hasPassage(b, a), "проход работает в обе стороны");
    t.check(m.passageCount() == 1, "счётчик проходов 1");
    m.connect(b, a);
    t.check(m.passageCount() == 1, "повторный проход не считается дважды");

    m.connect(Position{0, 0}, Position{1, 0});
    t.check(m.hasPassage(Position{1, 0}, Position{0, 0}) && m.passageCount() == 2, "вертикальный проход");
    t.check(!m.hasPassage(Position{0, 0}, Position{0, 1}), "другие стены не тронуты");

    bool thrown = false;
    try {
        m.connect(Position{0, 0}, Position{1, 1});
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    t.check(thrown, "проход по диагонали запрещён");

    thrown = false;
    try {
        m.connect(Position{0, 0}, Position{0, 2});
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    t.check(thrown, "проход через клетку запрещён");

    thrown = false;
    try {
        m.connect(a, a);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    t.check(thrown, "проход в саму себя запрещён");

    thrown = false;
    try {
        m.connect(Position{2, 2}, Position{2, 3});
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    t.check(thrown, "проход за границу запрещён");
    t.check(m.passageCount() == 2, "после ошибок проходов столько же");
}

void testGenerate(Tester& t) {
    std::cout << "Генерация:" << std::endl;

    int sizes[4][2] = { {2, 2}, {3, 7}, {10, 10}, {20, 15} };
    for (int i = 0; i < 4; i++) {
        Maze<int> m(sizes[i][0], sizes[i][1]);
        generateMaze(m, Position{0, 0}, RandomOrder(i + 1));
        std::string name = std::to_string(sizes[i][0]) + "x" + std::to_string(sizes[i][1]);
        t.check(isConnected(m), name + ": связный");
        t.check(m.passageCount() == m.cellCount() - 1, name + ": проходов на 1 меньше клеток");
        t.check(isPerfect(m), name + ": идеальный");
    }

    Maze<int> fixed(6, 6);
    generateMaze(fixed, Position{2, 3}, FixedOrder());
    t.check(isPerfect(fixed), "другой порядок соседей и старт не с угла: тоже идеальный");

    Maze<int> m1(8, 8);
    Maze<int> m2(8, 8);
    generateMaze(m1, Position{0, 0}, RandomOrder(5));
    generateMaze(m2, Position{0, 0}, RandomOrder(5));
    bool same = true;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c + 1 < 8; c++) {
            if (m1.hasPassage(Position{r, c}, Position{r, c + 1}) != m2.hasPassage(Position{r, c}, Position{r, c + 1})) {
                same = false;
            }
        }
    }
    t.check(same, "одинаковый seed даёт одинаковый лабиринт");

    bool thrown = false;
    try {
        generateMaze(m1, Position{0, 0}, RandomOrder(1));
    } catch (const std::logic_error&) {
        thrown = true;
    }
    t.check(thrown, "повторная генерация того же лабиринта запрещена");

    thrown = false;
    try {
        Maze<int> m3(3, 3);
        generateMaze(m3, Position{5, 5}, FixedOrder());
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    t.check(thrown, "старт генерации за границей - ошибка");
}

void testPath(Tester& t) {
    std::cout << "Поиск пути:" << std::endl;

    Maze<int> m(10, 12);
    generateMaze(m, Position{0, 0}, RandomOrder(7));
    Position start = {0, 0};
    Position finish = {9, 11};

    std::vector<Position> path = findPath(m, start, finish, ThroughPassages<int>(m));
    t.check(!path.empty(), "в идеальном лабиринте путь есть");
    t.check(isValidPath(m, path, start, finish), "путь идёт только через проходы");

    std::vector<Position> self = findPath(m, start, start, ThroughPassages<int>(m));
    t.check(self.size() == 1 && self[0] == start, "путь в саму себя - одна клетка");

    std::vector<Position> back = findPath(m, finish, start, ThroughPassages<int>(m));
    t.check(back.size() == path.size(), "обратный путь той же длины");

    Maze<int> walls(3, 3);
    t.check(findPath(walls, start, Position{2, 2}, ThroughPassages<int>(walls)).empty(),
            "без проходов пути нет");

    Maze<int> open(3, 3);
    openAll(open);
    std::vector<Position> shortest = findPath(open, start, Position{2, 2}, ThroughPassages<int>(open));
    t.check(shortest.size() == 5, "без стен путь кратчайший (5 клеток)");

    bool thrown = false;
    try {
        findPath(m, start, Position{10, 0}, ThroughPassages<int>(m));
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    t.check(thrown, "финиш за границей - ошибка");
}

void testPerfect(Tester& t) {
    std::cout << "Проверка идеальности:" << std::endl;

    Maze<int> empty(3, 3);
    t.check(!isConnected(empty) && !isPerfect(empty), "без проходов не связный и не идеальный");

    Maze<int> cycle(2, 2);
    openAll(cycle);
    t.check(isConnected(cycle), "все стены убраны - связный");
    t.check(!isPerfect(cycle), "есть цикл - не идеальный");

    Maze<int> half(2, 2);
    half.connect(Position{0, 0}, Position{0, 1});
    half.connect(Position{1, 0}, Position{1, 1});
    t.check(!isPerfect(half), "две отдельные части - не идеальный");
}

void testRules(Tester& t) {
    std::cout << "Свои правила и типы данных:" << std::endl;

    Maze<char> m(3, 3, ' ');
    openAll(m);
    m.setData(Position{1, 1}, 'X');
    std::vector<Position> path = findPath(m, Position{0, 0}, Position{2, 2}, AvoidTraps(m));
    t.check(!path.empty() && !inPath(path, Position{1, 1}), "путь обходит ловушку");

    Maze<char> blocked(2, 2, ' ');
    openAll(blocked);
    blocked.setData(Position{0, 1}, 'X');
    blocked.setData(Position{1, 0}, 'X');
    t.check(findPath(blocked, Position{0, 0}, Position{1, 1}, AvoidTraps(blocked)).empty(),
            "ловушки перекрыли путь - пути нет");
    t.check(!findPath(blocked, Position{0, 0}, Position{1, 1}, ThroughPassages<char>(blocked)).empty(),
            "с обычным правилом путь есть");

    Maze<std::string> names(4, 4, "комната");
    generateMaze(names, Position{0, 0}, RandomOrder(3));
    t.check(isPerfect(names), "лабиринт со строками в клетках тоже работает");
    t.check(names.getData(Position{3, 3}) == "комната", "данные клеток не испортились");
}

//  Демонстрация

void demo() {
    std::cout << std::endl << "===== Демонстрация =====" << std::endl;

    // первый лабиринт: выводим уровень и маршрут
    Maze<char> level(6, 10, ' ');
    generateMaze(level, Position{0, 0}, RandomOrder(42));
    Position start = {0, 0};
    Position finish = {5, 9};
    std::vector<Position> path = findPath(level, start, finish, ThroughPassages<char>(level));

    std::cout << "Лабиринт 6x10, маршрут из " << start << " в " << finish << ":" << std::endl;
    printMaze(std::cout, level, start, finish, path);
    std::cout << "Длина маршрута: " << path.size() << " клеток" << std::endl;
    std::cout << "Идеальный: " << (isPerfect(level) ? "да" : "нет") << std::endl;

    // второй лабиринт другого размера
    Maze<int> big(15, 25, 0);
    generateMaze(big, Position{7, 12}, RandomOrder(2024));
    std::vector<Position> bigPath = findPath(big, Position{0, 0}, Position{14, 24}, ThroughPassages<int>(big));
    std::cout << std::endl << "Лабиринт 15x25:" << std::endl;
    std::cout << "Клеток: " << big.cellCount() << ", проходов: " << big.passageCount() << std::endl;
    std::cout << "Идеальный: " << (isPerfect(big) ? "да" : "нет") << std::endl;
    std::cout << "Длина пути из угла в угол: " << bigPath.size() << " клеток" << std::endl;

    // правило с ловушками на лабиринте без стен
    Maze<char> traps(3, 4, ' ');
    openAll(traps);
    traps.setData(Position{0, 1}, 'X');
    traps.setData(Position{1, 1}, 'X');
    std::vector<Position> safe = findPath(traps, Position{0, 0}, Position{0, 3}, AvoidTraps(traps));
    std::cout << std::endl << "Комната 3x4 без стен, ловушки в (0, 1) и (1, 1):" << std::endl;
    printMaze(std::cout, traps, Position{0, 0}, Position{0, 3}, safe);
    std::cout << "Безопасный путь: ";
    printPath(std::cout, safe);
}

int main() {
#ifdef _WIN32
    system("chcp 65001 > nul");  
#endif

    Tester t;
    testCreate(t);
    testNeighbors(t);
    testPassages(t);
    testGenerate(t);
    testPath(t);
    testPerfect(t);
    testRules(t);
    t.report();

    demo();

    return t.allPassed() ? 0 : 1;
}
