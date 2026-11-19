#include <iostream>
#include <cmath>
#include <stdexcept>
#include <vector>
#include <memory>
#include <string>
#include <cstdlib>

const double EPS = 1e-9;


// после создания не меняется
class Complex {
public:
    Complex(double re = 0, double im = 0) : _re(re), _im(im) {}

    double getRe() const { return _re; }
    double getIm() const { return _im; }

    Complex operator+(const Complex& other) const {
        return Complex(_re + other._re, _im + other._im);
    }

    Complex operator-(const Complex& other) const {
        return Complex(_re - other._re, _im - other._im);
    }

    // (a+bi)(c+di) = (ac-bd) + (ad+bc)i
    Complex operator*(const Complex& other) const {
        double re = _re * other._re - _im * other._im;
        double im = _re * other._im + _im * other._re;
        return Complex(re, im);
    }

    // умножаем верх и низ на сопряженное к знаменателю
    Complex operator/(const Complex& other) const {
        double d = other._re * other._re + other._im * other._im;
        if (d < EPS) {
            throw std::invalid_argument("деление на ноль");
        }
        double re = (_re * other._re + _im * other._im) / d;
        double im = (_im * other._re - _re * other._im) / d;
        return Complex(re, im);
    }

    bool operator==(const Complex& other) const {
        return std::fabs(_re - other._re) < EPS && std::fabs(_im - other._im) < EPS;
    }

    bool operator!=(const Complex& other) const {
        return !(*this == other);
    }

private:
    double _re;
    double _im;
};

std::ostream& operator<<(std::ostream& out, const Complex& c) {
    double re = c.getRe();
    double im = c.getIm();
    // убираем мусор вида 1e-17 и -0
    if (std::fabs(re) < EPS) re = 0;
    if (std::fabs(im) < EPS) im = 0;

    out << re;
    if (im < 0) {
        out << "-";
    } else {
        out << "+";
    }
    out << std::fabs(im) << "i";
    return out;
}


// чтобы не было + -5
void printSigned(std::ostream& out, double value) {
    if (value < 0) {
        out << " - " << -value;
    } else {
        out << " + " << value;
    }
}

// общий тип для всех уравнений
class Equation {
public:
    virtual ~Equation() = default;

    // значение f(x) в точке
    virtual Complex operator()(const Complex& x) const = 0;

    // корни уравнения
    virtual std::vector<Complex> solve() const = 0;

    // вывод уравнения в виде текста
    virtual void print(std::ostream& out) const = 0;
};

std::ostream& operator<<(std::ostream& out, const Equation& eq) {
    eq.print(out);
    return out;
}

// ax + b = 0
class LinearEquation : public Equation {
public:
    LinearEquation(double a, double b) : _a(a), _b(b) {
        if (std::fabs(a) < EPS) {
            throw std::invalid_argument("в линейном уравнении a не может быть 0");
        }
    }

    Complex operator()(const Complex& x) const override {
        return Complex(_a) * x + Complex(_b);
    }

    std::vector<Complex> solve() const override {
        std::vector<Complex> roots;
        roots.push_back(Complex(-_b / _a));
        return roots;
    }

    void print(std::ostream& out) const override {
        out << _a << "x";
        printSigned(out, _b);
        out << " = 0";
    }

private:
    double _a;
    double _b;
};

// ax^2 + bx + c = 0
class QuadraticEquation : public Equation {
public:
    QuadraticEquation(double a, double b, double c) : _a(a), _b(b), _c(c) {
        if (std::fabs(a) < EPS) {
            throw std::invalid_argument("в квадратном уравнении a не может быть 0");
        }
    }

    Complex operator()(const Complex& x) const override {
        return Complex(_a) * x * x + Complex(_b) * x + Complex(_c);
    }

    std::vector<Complex> solve() const override {
        std::vector<Complex> roots;
        double d = _b * _b - 4 * _a * _c;
        double re = -_b / (2 * _a);

        if (std::fabs(d) < EPS) {
            // один корень (два одинаковых)
            roots.push_back(Complex(re));
        } else if (d > 0) {
            // два действительных корня
            double shift = std::sqrt(d) / (2 * _a);
            roots.push_back(Complex(re - shift));
            roots.push_back(Complex(re + shift));
        } else {
            // два комплексных корня
            double im = std::sqrt(-d) / (2 * _a);
            roots.push_back(Complex(re, im));
            roots.push_back(Complex(re, -im));
        }
        return roots;
    }

    void print(std::ostream& out) const override {
        out << _a << "x^2";
        printSigned(out, _b);
        out << "x";
        printSigned(out, _c);
        out << " = 0";
    }

private:
    double _a;
    double _b;
    double _c;
};


// коллекция владеет уравнениями (unique_ptr сам удалит их)
class EquationList {
public:
    void add(std::unique_ptr<Equation> eq) {
        if (!eq) {
            throw std::invalid_argument("нельзя добавить пустое уравнение");
        }
        _items.push_back(std::move(eq));
    }

    size_t size() const {
        return _items.size();
    }

    const Equation& get(size_t index) const {
        if (index >= _items.size()) {
            throw std::out_of_range("нет уравнения с таким номером");
        }
        return *_items[index];
    }

    // корни каждого уравнения
    std::vector<std::vector<Complex>> solveAll() const {
        std::vector<std::vector<Complex>> result;
        for (size_t i = 0; i < _items.size(); i++) {
            result.push_back(_items[i]->solve());
        }
        return result;
    }

    // значение каждого уравнения в точке x
    std::vector<Complex> valuesAt(const Complex& x) const {
        std::vector<Complex> result;
        for (size_t i = 0; i < _items.size(); i++) {
            const Equation& eq = *_items[i];
            result.push_back(eq(x));
        }
        return result;
    }

    // номера уравнений, у которых x корень (проверяем f(x) == 0)
    std::vector<size_t> withRoot(const Complex& x) const {
        std::vector<size_t> result;
        for (size_t i = 0; i < _items.size(); i++) {
            const Equation& eq = *_items[i];
            if (eq(x) == Complex(0)) {
                result.push_back(i);
            }
        }
        return result;
    }

private:
    std::vector<std::unique_ptr<Equation>> _items;
};

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

void testComplex(Tester& t) {
    std::cout << "Комплексные числа:" << std::endl;
    Complex a(1, 2);
    Complex b(3, -1);

    t.check(a + b == Complex(4, 1), "сложение");
    t.check(a - b == Complex(-2, 3), "вычитание");
    t.check(a * b == Complex(5, 5), "умножение");
    t.check(a / b == Complex(0.1, 0.7), "деление");
    t.check(Complex(0, 1) * Complex(0, 1) == Complex(-1), "i * i = -1");
    t.check(a == Complex(1, 2), "равенство");
    t.check(a != b, "неравенство");
    t.check(!(a != Complex(1, 2)), "!= для равных чисел даёт false");
    t.check(Complex(0.1) + Complex(0.2) == Complex(0.3), "сравнение с погрешностью 0.1+0.2 == 0.3");
    t.check(Complex(5) == Complex(5, 0), "конструктор с одним числом");
    t.check(Complex() == Complex(0, 0), "конструктор по умолчанию это 0");

    Complex sum = a + b;
    t.check(a == Complex(1, 2) && b == Complex(3, -1) && sum == Complex(4, 1),
            "операции не меняют исходные числа");

    bool thrown = false;
    try {
        a / Complex(0, 0);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    t.check(thrown, "деление на ноль бросает исключение");
}

void testLinear(Tester& t) {
    std::cout << "Линейное уравнение:" << std::endl;
    LinearEquation eq(2, 4);   // 2x + 4 = 0

    t.check(eq(Complex(0)) == Complex(4), "f(0) = 4");
    t.check(eq(Complex(-2)) == Complex(0), "f(-2) = 0");
    t.check(eq(Complex(0, 1)) == Complex(4, 2), "f(i) = 4+2i");

    std::vector<Complex> roots = eq.solve();
    t.check(roots.size() == 1, "один корень");
    t.check(roots[0] == Complex(-2), "корень равен -2");

    LinearEquation neg(-3, 0);   // -3x = 0
    t.check(neg.solve()[0] == Complex(0), "-3x = 0 даёт корень 0");

    bool thrown = false;
    try {
        LinearEquation bad(0, 5);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    t.check(thrown, "a = 0 не даёт создать уравнение");
}

void testQuadratic(Tester& t) {
    std::cout << "Квадратное уравнение:" << std::endl;

    QuadraticEquation two(1, -5, 6);   // x^2 - 5x + 6, корни 2 и 3
    std::vector<Complex> r1 = two.solve();
    t.check(r1.size() == 2, "D > 0: два корня");
    t.check(r1[0] == Complex(2) && r1[1] == Complex(3), "корни 2 и 3");
    t.check(two(Complex(1)) == Complex(2), "f(1) = 2");

    QuadraticEquation one(1, -2, 1);   // (x-1)^2, корень 1
    std::vector<Complex> r2 = one.solve();
    t.check(r2.size() == 1, "D = 0: один корень");
    t.check(r2[0] == Complex(1), "корень 1");

    QuadraticEquation cplx(1, 0, 1);   // x^2 + 1, корни i и -i
    std::vector<Complex> r3 = cplx.solve();
    t.check(r3.size() == 2, "D < 0: два комплексных корня");
    t.check(r3[0] == Complex(0, 1) && r3[1] == Complex(0, -1), "корни i и -i");
    t.check(cplx(Complex(0, 1)) == Complex(0), "f(i) = 0");

    QuadraticEquation negA(-1, 0, 4);  // -x^2 + 4, корни 2 и -2
    std::vector<Complex> r4 = negA.solve();
    t.check(r4.size() == 2 && negA(r4[0]) == Complex(0) && negA(r4[1]) == Complex(0),
            "отрицательное a: корни подходят");

    bool thrown = false;
    try {
        QuadraticEquation bad(0, 1, 1);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    t.check(thrown, "a = 0 не даёт создать уравнение");
}

void testList(Tester& t) {
    std::cout << "Коллекция уравнений:" << std::endl;

    EquationList empty;
    t.check(empty.size() == 0, "пустая коллекция");
    t.check(empty.solveAll().empty(), "решение пустой коллекции ничего не даёт");
    t.check(empty.withRoot(Complex(1)).empty(), "поиск корня в пустой коллекции");

    EquationList list;
    list.add(std::make_unique<LinearEquation>(2, 4));          // 0: корень -2
    list.add(std::make_unique<QuadraticEquation>(1, 0, 1));    // 1: корни i, -i
    list.add(std::make_unique<QuadraticEquation>(1, -5, 6));   // 2: корни 2, 3
    t.check(list.size() == 3, "добавлено 3 уравнения");

    std::vector<std::vector<Complex>> all = list.solveAll();
    t.check(all.size() == 3, "решены все 3");
    t.check(all[0].size() == 1 && all[1].size() == 2 && all[2].size() == 2, "у каждого своё число корней");

    // каждый найденный корень должен давать f(x) = 0
    bool rootsOk = true;
    for (size_t i = 0; i < all.size(); i++) {
        for (size_t j = 0; j < all[i].size(); j++) {
            if (list.get(i)(all[i][j]) != Complex(0)) {
                rootsOk = false;
            }
        }
    }
    t.check(rootsOk, "все найденные корни подходят при подстановке");

    std::vector<Complex> values = list.valuesAt(Complex(2));
    t.check(values.size() == 3, "значения посчитаны для всех");
    t.check(values[0] == Complex(8) && values[1] == Complex(5) && values[2] == Complex(0),
            "значения в x = 2: 8, 5, 0");

    std::vector<size_t> found = list.withRoot(Complex(2));
    t.check(found.size() == 1 && found[0] == 2, "x = 2 корень только у третьего");

    std::vector<size_t> foundI = list.withRoot(Complex(0, 1));
    t.check(foundI.size() == 1 && foundI[0] == 1, "x = i корень только у второго");

    t.check(list.withRoot(Complex(100)).empty(), "x = 100 не корень ни у кого");

    list.add(std::make_unique<LinearEquation>(1, -2));   // 3: x - 2, корень 2
    std::vector<size_t> found2 = list.withRoot(Complex(2));
    t.check(found2.size() == 2 && found2[0] == 2 && found2[1] == 3, "x = 2 корень у двух уравнений");

    bool thrown = false;
    try {
        list.add(nullptr);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    t.check(thrown, "пустое уравнение добавить нельзя");

    thrown = false;
    try {
        list.get(100);
    } catch (const std::out_of_range&) {
        thrown = true;
    }
    t.check(thrown, "обращение к несуществующему номеру");
}

//  Демонстрация 

void demo() {
    std::cout << std::endl << "===== Демонстрация =====" << std::endl;

    EquationList list;
    list.add(std::make_unique<LinearEquation>(2, 4));
    list.add(std::make_unique<QuadraticEquation>(1, 0, 1));
    list.add(std::make_unique<QuadraticEquation>(1, -5, 6));
    list.add(std::make_unique<QuadraticEquation>(1, -2, 1));

    std::cout << "Уравнения:" << std::endl;
    for (size_t i = 0; i < list.size(); i++) {
        std::cout << "  " << i << ") " << list.get(i) << std::endl;
    }

    std::cout << "Корни:" << std::endl;
    std::vector<std::vector<Complex>> all = list.solveAll();
    for (size_t i = 0; i < all.size(); i++) {
        std::cout << "  " << i << ") ";
        for (size_t j = 0; j < all[i].size(); j++) {
            std::cout << "x" << j + 1 << " = " << all[i][j] << "   ";
        }
        std::cout << std::endl;
    }

    Complex x(2);
    std::cout << "Значения в точке x = " << x << ":" << std::endl;
    std::vector<Complex> values = list.valuesAt(x);
    for (size_t i = 0; i < values.size(); i++) {
        std::cout << "  " << i << ") " << values[i] << std::endl;
    }

    std::cout << "x = " << x << " является корнем у:" << std::endl;
    std::vector<size_t> found = list.withRoot(x);
    for (size_t i = 0; i < found.size(); i++) {
        std::cout << "  " << list.get(found[i]) << std::endl;
    }

    std::cout << "Пробуем создать 0x + 5 = 0: ";
    try {
        LinearEquation bad(0, 5);
    } catch (const std::exception& e) {
        std::cout << "ошибка: " << e.what() << std::endl;
    }
}

int main() {
#ifdef _WIN32
    system("chcp 65001 > nul");   // чтобы русский текст нормально выводился в консоли Windows
#endif

    Tester t;
    testComplex(t);
    testLinear(t);
    testQuadratic(t);
    testList(t);
    t.report();

    demo();

    return t.allPassed() ? 0 : 1;
}
