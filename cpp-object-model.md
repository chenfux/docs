### 对象模型的理解

文档迁移中...

<!--
静态类型: 指针、引用、变量声明时的类型,编译期决定.
动态类型: 指针、引用运行时内存中的实际类型,运行期决定.

//1. 对成员函数取地址的含义及.*和->*的含义. 参考[成员指针-继承条件下的成员指针操作]

non-static non-virtual 成员函数:
    取到的地址是真实的函数调用入口地址,调用时依赖于对象调用(通过.*和->*执行).
    这是因为成员函数被调用时,编译器会隐式的将调用对象做为this指针传递到成员函数内,所以成员函数内就可以访问对象的所有成员.
    而如果通过一个const对象来调用成员函数,那么被传递到成员函数中的this指针也将是带有const限定的,这就是const成员函数中也只能调用其他const成员的原因.
virtual 成员函数:
    取到的是该virtual成员函数被安排到vtab中的索引(也可能取到的是一个结构体,结构体中的某个字段包含了该索引信息),调用时依赖于对象.
    因为除了要将调用对象隐式传递到成员函数内作为this指针外, 还要通过对象内部的vptr来找到vtab, 然后根据vitrual成员函数位于vtab中的索引值来调用函数.
static 成员函数:
    取到的地址是真实的函数调用入口地址,调用时不需要依赖于对象,使用时与普通的函数指针无异.


//2.继承基类的成员函数、隐藏基类的成员函数、重写基类的成员函数以及多态调用
    2.1 子类继承了基类的成员函数: 
        non-virtual函数: 本质上是根据静态类型进行名称查找时没有在子类找到这个函数名, 进而继续查找基类,并在基类中找到了这个所谓的继承函数. 之后通过子类对象调用的时候也都是调用的基类中这些函数.
        如果这个函数是virtual函数,且发生了多态调用: 
        struct Base
        {
            virtual void member(){ /**/ }
        };
        struct Derive : public Base{ };

        Base* p = new Derive();
        p->member();
        delete p;

        对于"p->member()"这个表达式,本质上是从Base类的虚表中获取member()的索引值, 然后从派生类的虚表中调用该索引的函数指针,
        而因为子类是没有重写member(), 所以Derive类的vtab中这个索引依然指向的Base::member()的实现,这就是继承下的virtual调用.
        (如果子类重写了member()方法, 那么子类的vtab中的这个索引就指向被重写后的Derive::member()的实现, 
        所以更明确的说, 多态调用其实是从基类的vtab中找到要被调用的那个虚函数的索引, 然后再从子类的vtab中根据这个索引的指向来调用).

    再看一个类似的例子:

#include <stdio.h>

struct B {
    virtual void f(){ printf("B\n"); }
    virtual ~B(){}
};
struct D : B {
    virtual void f(int){ printf("D\n"); }
};

int main( int argc, char* argv[] )
{

    B b;
    D d;
    B* p = &d;  

    b.f();  //B
    p->f(); //B   //获取到f位于B的vtab中的索引位置, 然后再从D的虚表中找到这个位置（其实B和A的虚表都是一个, 只是虚表的大小不同， 如果子类没有新的virtual函数, 子类的虚表和父类的虚表大小是一样的， 只是需要中的函数指针指向的函数是不同的， 如果子类有新的virtual函数, 那么这个虚表要扩大一些索引, 来容纳自类的virtual函数）， 并调用。因为D并没有重写它, 所以这个位置的指针还是只想B类的实现.
    d.f(1); //D


    return 0;
}



class test1
{
    int v1;
public:
    void __test1(){};
};

class test2  : public test1
{
    int v2;
public:
    void __test2(){};
};

int main( int argc,  char* argv[] )
{
    test1* p = new test2();
    //p->__test2(); //编译失败.
    //(*p).__test2(); //编译失败.
    p->__test1();

   //上面两个编译失败就是因为在名称查找的时候在p的静态类型(test1)中根本找不到__test2这个成员.

    return 0;
}



 看一个复杂点的例子:

#include <iostream>
using namespace std;

class base
{
public:
    virtual void func(int){ cout << "base::func(int)" << endl;}
};

class test : public base
{
public:
    virtual void func(float) { cout << "test::func(float)" << endl; }
    virtual void func(int) { cout << "test::func(int)" << endl; }

};


int main( int argc, char* argv[] )
{

    base* p = new test();
    test t;

    p->func(1); //test::func(int)
    p->func(1.0f); //test::func(int)
    t.func(1); //test::func(int)
    t.func(1.0f); //test::func(float)

    return 0;
}

看第二个输出, 它竟然是调用的是test::func(int), 而不是test::func(float), 可是p明明是一个test类型对象, 而且test类中也有一个接收float类型为参数的func成员函数啊, 但是为什么还是调用的test::func(int)呢?

上面说过,多态调用的本质是从基类的vtab中查找被调用的那个virtual函数的索引值, 那也就是说多态情况下的virtual函数调用,基类中必须要存在这个被调用的virtual函数, 否则, 无法从基类中获取到被调用函数位于基类虚表中的索引值, 也就无法完成多态调用.
在这个例子中就是这样的, 基类就不存在要被调用的那个virtual函数(接收float参数的virtual function),正常情况下是不能编译通过的, 然而, 意外的是, 实参1.0f却可以在隐式转型之后匹配到base::test(int), 而这个virtual function在基类中是存在的,
所以, 最终匹配到base::func(int), 所以才没有导致失败, 如果我们以一个不可向int转型的实参来进行调用, 则将导致编译失败. 
比如: p->func("1.0f");
将引发下面的编译错误: error: invalid conversion from 'const char*' to 'int' ;

上面这些实际上是深入一些的分析, 而更容易记忆一个的原则是: 
    多态的virtual函数调用在编译期根据静态类型来决策, 运行期根据动态类型来决策.


    2.2 子类隐藏基类的成员函数实现: 实际上是根据静态类型进行名称查找规则优先找到派生类, 并找到了相应的名称, 从而停止名称查找, 所以表现出来的现象就是就是父类的实现被隐藏了.
    2.3 子类重写基类的virtual成员函数实现,且发生多态调用的时候: 
        struct Base()
        {
            virtual void member(){ /**/ }
        };
        struct Derive : public Base
        {
            virtual void member(){ /**/ }
        };

        Base* p = new Derive();
        p->member();
        delete p;

        对于"p->member()”这个表达式, 本质上是从Base类的虚表中获取member的索引值, 然后从Derive类的虚表中调用该索引的函数指针,
        而Derive类的vtab中的这个索引实际上已经被编译器重置为指向Derive::member(),这样就构成了多态调用.

//3. 为什么发生多态调用一定是通过基类的指针(或引用)指向一个子类才能实现呢?
因为实现多态调用的根本要求是内存中存放着一个派生类对象, 而恰好只能通过基类的指针和引用才能实现这个要求而已.
本质上而言,你可以用任意类型的指针指向一个派生类对象,然后做一个到基类指针的转型,一样可以调用到派生类的成员函数,语法上没啥实质性区别。 之所以必须要转换到基类只是因为这个virtual函数调用需要用到这个函数位于基类virtual tab中的索引.

说的更夸张一些,基类的指针或引用其实不需要关心内存中到底存着什么类型, 对成员函数调用的时候只需要按照以下行为来做:
    1. 按照静态类型进行成员函数名称查找,
    2. 如果在静态类型中没有找到这个函数名称, 则编译失败, 如果找到这个函数, 则判断它是否为virtual函数.
    3. 如果不是virtual函数, 则直接调用静态类型中的这个函数,此时不是多态调用.
    4. 如果是virtual函数, 则获取到这个函数位于静态类型的vtab中的索引值(假设为n), 然后, 从内存中的这个对象里去找vptr, 通过vptr再去找这个对象所属类型的vtab,
    5. 从这个vtab中调用对应索引的那个函数: vtab[n]();
       如果内存中的对象是基类本身(Base* p = new Base()), 此刻, vtab[n]自然就是指向基类自己的这个virtual函数. 
       如果内存中的对象是子类, 且子类没有重写这个virtual函数, 此刻, 子类的vtab[n]指向的就是基类的这个virtual函数.
       如果内存中的对象是子类, 且子类重写了这个virtual函数, 此刻, 子类的vtab[n]指向的就是子类重写过的这个virtual函数.
    
    //注: vtab可以简单认为是函数指针数组.

上面就是多态调用的步骤,为了保证上面的这些步骤得以按照预期执行, 下面就是前提:
1. 让基类指针指向一个派生类对象.
2. 修改子类vtab中的函数指针指向被子类重写过的virtual函数实现,同时修改子类对象中的vptr指向子类的vtab(编译器的工作).


具体分析下面的这几行代码说明(member被子类重写过):
Derive d = new Derive();  //1.
Base   b = d; //2.
Base*  p = new Derive(); //3
Base&  r = d; //4.

d.member();  //Derive::member()
b.member();  //Base::member();
p->member(); //Derive::member();
r.member();  //Derive::member();


delete d;
delete p;


表达式1: 运行期它内存中存在的对象就就是本类对象, 因此vtab中的某个索引就指向本类的member实现,所以调用的是Derive::member(),
表达式2: 这里会发生“截断",即这个赋值操作只是将派生类中的基类部分赋值到b对象中去了,也就是b的内存中始终是Base类型的对象,vtab中的member实现也是Base::member()：
表达式3: 通过指针的方式可以指向一个派生类对象, 运行期内存中就是一个派生类对象,这意味着这个对象中的vptr指向的是子类的虚表,那么member成员也当然就是子类重新实现的版本.
表达式4: 类似于表达式3. 



//4. p->func()和(*p).func()的区别。
http://stackoverflow.com/questions/7420314/c-iso-standard-interpretation-of-dereferencing-pointer-to-base
struct Base
{
    virtual void func() const { printf("Base::func()\n"); }
};

struct Derived : public Base
{
    virtual void func() const { printf("Derived::func()\n"); }
};

int main( int argc, char* argv[] )
{
    Base* p = new Derived();
    p->func();   //Derived::func()
    (*p).func(); //Derived::func()
}

//p->func() 和 (*p).func() 前者实际上一个是通过指针发起的多态调用, 后者是通过引用发起的多态调用.
//不要以为(*p)是一个Base类型的对象, 它其实是一个Base类型的引用(Base&), 这意味这它仍然引用这内存中的那个子类对象.


//5. 如果一个纯虚函数被给出了实现,则这个纯虚函数允许被静态调用(通过类名调用).
#include <stdio.h>

class abstract
{
public:
    virtual const char* self() const = 0;
};

const char* abstract::self() const { return "abstract"; }

class test : public abstract
{
public:
    virtual const char* self() const { return "test"; }
    virtual const char* base() const { return abstract::self(); }
};


int main( int argc, char* argv[] )
{
    test t;
    printf("%s\n", t.base());
    printf("%s\n", t.self());

    return 0;
};



### 成员指针基本使用


/**
 * 成员指针的基本使用.
 */

//1.
#include <iostream>
#include <typeinfo>
using namespace std;

struct Date
{
    int year;
    int month;
    int day;
    void show() const { cout << this->year << '-' << this->month << '-' << this->day << endl; }
};


//const成员函数指针的const修饰符的位置,与成员函数的位置一样,都是放在声明的最后.
void showArray( Date* d, int n, void(Date::* _show)() const )
{
    for( int i = 0; i < n; i++ )
    {
        (d[i].*_show)();
    }
}

void showArray( Date* d, int n, int Date::* year, int Date::* month, int Date::* day )
{
    for( int i = 0; i < n; i++ )
    {
        cout << d->*year << '-' << d->*month << '-' << d->*day << endl;
        ++d;
    }
}

 int main()
 {
    Date now[3] = { {1995, 2, 21}, {2015, 7, 1}, { 2020, 2, 21 } };

    //成员指针的定义与普通指针类似,只是成员指针需要在”*“之前加上类名称限定,标识当前这个成员指针所属的类,同时,取成员地址的时候也要限定出成员所属的类.
    //对于成员变量而言,取它们的地址意味着取到的是它们各自距离对象起始内存的相对偏移量.
    int Date::* pyear  = &Date::year;
    int Date::* pmonth = &Date::month;
    int Date::* pday   = &Date::day;

    //对于所有的成员指针(静态成员指针除外),在解引用的时候都需要通过"成员指针所属类类型"的对象使用.*和->*来完成 [多态-实现原理]
    showArray( now, 3, pyear, pmonth, pday );
    showArray( now, 3, &Date::show );
    return 0;
 }


//2.
#include <stdio.h>
#include <string>
using namespace std;

class T
{
public:
    T( float __x = 0.0, float __y = 0.0, float __z = 0.0 ):x(__x),y(__y),z(__z){}
public:
    float x, y, z;
};



int main()
{

   /*
    * 注意"&T::x"和"&t.x"的区别:
    * "&T::x":
    * 1. 通过这种形式取地址得到的是成员变量相对于class object起始地址的偏移量.
    * 2. 类型float T::*
    * "&t.x":
    * 1. 通过这种形式取地址得到的是成员变量位于内存的真实地址.
    * 2. 类型是 float*
    * */
    T t(1.23,4.56, 7.89 );
    printf("t = %p\n", &t );
    printf("t.x = %p\n", &t.x );
    printf("t.y = %p\n", &t.y );
    printf("t.z = %p\n", &t.z );
    printf("T::x = %p\n", &T::x );
    printf("T::y = %p\n", &T::y );
    printf("T::z = %p\n", &T::z );

    float* y = &t.y;
    printf("*y = %.2f\n", *y ); //4.56;



    return 0;
}




### 静态成员指针


#include <stdio.h>

/**
  * 静态成员指针的使用规则.
  */


class CC
{
 public:
    static void show() { printf("show()\n"); }
 public:
    static int sInt;
};
int CC::sInt = 0;

int main()
{



    //因为静态成员属于类作用域,所以不需要关联对象,因此静态成员指针存储的并不是相对于对象起始地址的偏移量,而是真正的有效地址;
    //所以,直接用相同类型的指针就可以指向类的静态成员指针,但在取静态成员地址的时候要明确指出其所属的类作用域.
    int *psInt = &CC::sInt;
    void (*pFunc)() = &CC::show;


    //想下面这样调用就是错误的, 静态成员sInt和show的类型分别是 int* 和 void(*)(), 而不是 int CC::* 和 void(C::*)().
    //更深的原因是: 标识了成员函数的指针在被解引用的时候需要将调用对象隐式的传递作为this参数, 而静态成员不允许/不需要/不可能包含this指针.
    //int CC::* psInt = &CC::sInt;       //编译失败;
    //void( CC::* pFunc )() = &CC::show; //编译失败;


    //使用静态成员指针时与普通指针一模一样.
    printf("sInt = %d\n", *psInt );
    pFunc();

    return 0;
}





### 继承条件下成员函数指针的正确使用以及要注意的问题.

#include <iostream>
#include <typeinfo>
using namespace std;
class Base
{
public:
    virtual void show(){ cout << "Base::show" << endl; }
    void print() { cout << "hello world" << endl; }
};

class Derive : public Base
{
public:
    virtual void show(){ cout << "Derive::show" << endl; }
};
int main()
{
    //基类成员函数可以赋于派生类成员函数指针(任何情况下都不会出错),
    //这是因为任何一个派生类对象内部都含有一个基类子对象, 因此所有对基类对象的合法操作同时对子类对象也是合法的.
    void(Derive::*pdf)()     = &Base::show;


    //派生类未重写/隐藏基类public成员函数的情况下也可以将该成员函数赋值给基类成员函数指针(也可以反过来),
    //这种情况下意味这派生类的这个成员函数是完全原模原样的继承自基类,它们本身就是一个,所以允许相互赋值操作.
    void(Derive::*pdprint)() = &Base::print;
    void(Base::*pbprint)()   = &Derive::print;


    //派生类重写过的成员函数就不能被赋值到基类的成员函数指针.
    //对一个成员函数调用时,调用对象将会被作为this指针隐式的传到函数内,
    //因此,如果我们将一个子类的nonvirtual成员函数赋值给基类的成员函数指针如果成立的话,
    //这将意味着当基类对象调用这个成员函数的时候,作为this传递进成员函数的实际是基类对象,
    //那么,如果在函数内部用到了子类特有的数据成员呢(因为这个函数本身就是子类的,用到子类的特有成员合情合理),将是非法语义.
    //void(Base::*pbf)()       = &Derive::show; //编译失败: error: cannot convert 'void (Derive::*)()' to 'void (Base::*)()' in initialization


    Base* bd = new Derive();
    void(Base::*pbf2)()   = &Base::show;
    void(Derive::*pdf2)() = &Derive::show;


     //bd实际是是一个Derive类型的对象,也就是说bd内存中的vptr指向的是Derive类的vtab,
     //因此通过对成员函数指针解引用所得到的虚表索引在子类的vtab中对应的实际上是子类的实现,所以调用的就是子类的show().
    (bd->*pbf2)(); //发生多态调用, 输出:Derive::show



    //编译错误(可能的原因): 因为编译期bd只被认为是Base类型, 而它要解引用的成员指针所属类型却是Derive(虽然bd在运行期确实是Derive类型的,但是编译期是无法确定的),
    //可能的原因: 通过Derive的成员函数指针来解引用可能得到一个对于Base类的vtab而言非法的索引值.
    //(bd->*pdf2)();

    delete bd;

  return 0;
 }


 ### 成员名称查找

 /**
相关参考： [对象模型-细节]
*/

//1. 
#include <iostream>
using namespace std;
class IRectangle
{
public:
    virtual ~IRectangle() {}
    virtual void Draw() = 0;
};
class Rectangle: public IRectangle
{
public:
    virtual ~Rectangle() {}
    virtual void Draw(int scale)
    {
        cout << "Rectangle::Draw(int)" << endl;
    }
    virtual void Draw()
    {
        cout << "Rectangle::Draw()" << endl;
    }
};
int main(void)
{
    IRectangle *pI = new Rectangle;
    pI->Draw();

    //编译失败,根据静态类型进行名称查找失败,IRectangle中没有带int参数的Draw方法.
    pI->Draw(200); 
    delete pI;
    return 0;
}


//2. 
#include <iostream>
using namespace std;
class Animal
{
public:
       void sleep(){ cout<<"Animal sleep"<<endl; }   
       void breathe(){ cout<<"Animal breathe"<<endl; }  //注意: 非虚函数
};
class Fish:public Animal
{
public:
       void breathe(){ cout<<"Fish bubble"<<endl; }     //注意: 非虚函数
};


int main()
{
    Fish fh;
    Animal *pAn=&fh;
    //根据名称查找规则,优先查找Animal类,并且发现了名为breathe的空参函数,且是non-virtual,所以不需要考虑多态调用,因此直接调用的是基类的breathe()方法.
    pAn->breathe(); //Animal breathe
    return 0;
}


### 不要对数组元素使用多态


#include <stdio.h>
class Base
{
public:
    virtual void show() const { printf("Base::show()\n"); }
    virtual ~Base(){}
};

class Derive : public Base
{
public:
    virtual void show() const { printf("Derive::show()\n"); }
    virtual ~Derive(){}
private:
    //让子类对象和父类对象的大小不同.
    void* __p;
};

void show( const Base* arr, unsigned int size )
{
    //对一个指针__p进行加n操作(__p+n), 它的含义是:  __p + sizeof(*__p)*n,
    //在这个函数中, arr的每个元素实际的类型是Derive,arr+n的内存操作应该是 arr+sizeof(Derive)*n,
    //然而,很遗憾,sizeof计算的是静态类型的大小,即sizeof(Base), 所以这里的arr+n操作实际上是: arr+sizeof(Base)*n;
    //而且在这里例子中, sizeof(Derive) != sizeof(Base),所以这里arr+i将会完全的胡乱操作内存(把存着Derive对象的内存当作Base的对象来使用)。。。。
    for( int i = 0; i < size; ++i )
    {
        //arr[i].show();
        (arr + i)->show();
    }
}


int main( int argc, char* argv[] )
{
    printf("sizeof(void*) =%ld\n", sizeof(void*) );
    printf("sizeof(Base) = %ld\n", sizeof(Base) );
    printf("sizeof(Derive) = %ld\n", sizeof(Derive) );

    Base* arr = new Derive[10];
    ::show(arr,10);

    delete[] arr;
    return 0;
}



### 扩展

深度探索C++对象模型 -->
