#include <iostream>
#include <iomanip>

#pragma pack(push,1)
enum Place { EARTH = 1, MOON = 2, MARS = 3 };

struct Date {
    unsigned short nWeekDay : 3;
    unsigned short nMonthDay : 6;
    unsigned short nMonth : 5;
    unsigned short nYear : 8;
};

struct Foo {
    int a{};
    int b{};
    float c{};
    Place p{};
    int bf1 : 4;
    Place bf2 : 2;
    int rest : 26;
};

struct Bar {
    int d{};
    Foo* foo{};
    int m[4][3];
    union {
        Date date;
        unsigned int date_int{};
    };
};

struct Thing {
    int abc{};
};

struct RTTITest {
    virtual ~RTTITest(){};
};

struct A {
    virtual ~A() {}
};

struct B : A {
    virtual ~B() {}
};

struct C : B {
    virtual ~C() {}
};

struct D {
    virtual ~D() {}
};

struct E : C, D {
    virtual ~E() {}
};

struct Person {
    int age{20};
};

struct Student : Person {
    float gpa{3.9};
};

struct Faculty : Person {
    int wage{30000};
};

struct TA : Student, Faculty {
    int hours{40};
};

struct Baz : Bar {
    TA ta{};
    int e{};
    int thing{};
    int* f{};
    Foo g{};
    Thing* things{};
    char* hello{};
    wchar_t* wide_hello{};
    char intrusive_hello[32]{"hello, intrusive world!"};
    bool im_true{true};
    bool im_false{false};
    char im_also_true{7};
    __declspec(align(sizeof(void*))) RTTITest* rtti{};
    E* e_ptr{};
};
#pragma pack(pop)

int main(int argc, char* argv[]) {
    auto foo = new Foo{};
    foo->a = 42;
    foo->b = 1337;
    foo->c = 77.7f;
    foo->p = Place::MARS;
    foo->bf1 = Place::MOON;
    foo->bf2 = Place::MARS;
    foo->rest = 12345678;

    auto baz = new Baz{};
    baz->d = 123;
    baz->foo = foo;
    for (auto i = 0; i < 4; ++i) {
        for (auto j = 0; j < 3; ++j) {
            baz->m[i][j] = i + j;
        }
    }
    baz->date.nWeekDay = 1;
    baz->date.nMonthDay = 2;
    baz->date.nMonth = 3;
    baz->date.nYear = 4;
    baz->e = 666;
    baz->f = new int[10];
    for (auto i = 0; i < 10; ++i) {
        baz->f[i] = i;
    }
    baz->g = *foo;
    ++baz->g.a;
    ++baz->g.b;
    ++baz->g.c;
    baz->g.p = Place::MOON;
    baz->things = new Thing[10];
    for (auto i = 0; i < 10; ++i) {
        baz->things[i].abc = i * 2;
    }
    baz->hello = (char*)"Hello, world!";
    baz->wide_hello = (wchar_t*)L"Hello, wide world!";

    auto rtti = new RTTITest{};
    baz->rtti = rtti;
    baz->e_ptr = new E{};

    std::cout << "0x" << std::hex << (uintptr_t)baz << std::endl;
    std::cout << "Press ENTER to exit.";
    std::cin.get();
    return 0;
}