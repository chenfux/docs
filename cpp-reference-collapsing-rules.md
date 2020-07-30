# 引用折叠规则 (reference collapsing rules)

文档迁移中...

<!--

当实参为 const char* 类型时, 如果函数模板形参声明为 T&&，
template <typename T>
void fun( T&& ) {}

const char* s = "123.123.123.123";
func(s); //T的类型为 const char* &
func("123.123.123.123);  //T的类型为 const char(&)[16]


当实参为const char* 类型时, 如果函数模板形参声明为 T&，
template <typename T>
void fun( T& ) {}

const char* s = "123.123.123.123";
func(s); //T的类型为 const char*
func("123.123.123.123);  //T的类型为 const char [16]



//1. 函数模板推导的限制: 无法通过一个受限的类型名称来推导模板参数.
e.g.
template<typename T>
void func( typename name<T>::type value ) {/**/}  //无法推导模板参数T.

类似的情况参考 std::forward 实现.


//2. 函数模板的模板参数推导过程中不考虑隐式转型(参见<Effective C++ 3rd> 条款46)


//2.
引用折叠规则:
函数形参声明(T是模板参数) | 函数实参类型(A是实参类型) | 模板参数替换之后的实际形参类型
T A A
T A& A
T A&& A
T& A A&
T& A& A&
T& A&& A&
T&& A A&&
T&& A& A&
T&& A&& A&&




第一部分: 实参演绎类型 _Type&

#include <iostream>
using namespace std;

template <typename _Type>
void DeductionType( _Type& __type ) { __type.Method(); }
//在函数实现者的角度而言, 将参数设置为一个_Type&类型,而没有对其进行const的修饰,
//这就给函数调用者传递了一个信息: 在这个函数的内部(可能)会对参数进行修改;

//测试1: 左值,编译通过;
int main()
{
     int n = 100;
     DeductionType( n ); //编译通过;
}

//模板参数_Type被推导为int类型, 函数形参的完整类型为int&;


//测试2: 带有cv修饰符的左值,类型推导保留cv限定;
int main()
{
    const int n = 100;
    //volatile int n = 100;
    DeductionType( n ); //编译通过;

}
/*
分析: 对于函数调用者而言,当使用一个const(volatile)变量做为实参进行函数调用时,我们的本意是希望函数不能修改这个变量,
(对于volatile限定类似, 只是我们希望程序每次都从内存中读取这个变量), 而形参声明中只是_Type&,没有进行cv限定
此时编译器为了保证我们希望的这些(const, volatile)语义,所以必须保留实参的cv限定;
所以最终模板参数_Type的类型被推导为const int, 而完整的函数形参声明为 const int&;
*/


//测试3: 不具名右值;
int main()
{
    //编译失败;
    DeductionType( 100 );

    //编译失败;
    DeductionType( int(100) );

}
/*
分析:
1. 关于参数推导: 对于测试3在调用时使用的实参类型为int&&,而形参模板类型声明为_Type&,
    这是因为函数在进行模板参数推导(实参演绎)时会发生引用折叠: _Type& + && = > _Type&,
    所以最终函数形参的完整类型为_Type&,而模板参数_Type被匹配为int类型;
2. 关于编译失败:
   由于函数的形参为非const左值引用,而实参为一个不具名右值,所以导致编译失败的原因类似于下面的表达式:
   int& __type = 100;
   int& __type = int(100);
   导致失败的原因是"非const左值引用不能绑定不具名右值":
   这是因为在不进行移动(move)语义的情况下,对一个不具名右值(临时变量,字面量)的修改是无意义的,思考下面的问题:
   对形参为非const左值引用的函数调用传递了一个临时变量, 首先函数的参数为非const左值引用就已经约定这个函数不会使用移动语义,但(可能)会在函数中修改参数,
   假设编译器允许编译通过,那么结果呢,函数调用结束之后,这个作为参数的临时变量被销毁,这次函数调用对该参数所做的所有修改都是无意义的;
   为避免这种语义的发生,所以在语法上添加这个限制;

注: 网上有很多人认为临时变量都是const的, 其实根本就不是, 那只是一种语义限制而已,这里有两个小例子:
template <typename _Type>
void DeductionType( _Type&& __type ) { __type.Method(); }
int main()
{
    //In instantiation of 'void DeductionType(_Type&&) [with _Type = int]':|
    DeductionType( int(100) );
    //In instantiation of 'void DeductionType(_Type&&) [with _Type = const int&]':|
    const int&& _C_Int_Reference = int(100);
    DeductionType( _C_Int_Reference );
    return 0;
}
从代码中第二条注释和上边的测试2可以看出:
无论是对于右值引用还是左值引用,编译器在推导模板参数类型的时候如果实参带有cv限定,则编译器一定会保留(参见测试2的分析);
可是,看第一条注释, 编译器将它推导为int类型(应用引用折叠规则: _Type&& + && => _Type&&, 所以_Type匹配为int),
说明它本身就不具备cv限定(至少编译器都不认为它具备cv限定),何来const之说;
注: 如果对两条注释中的_Type类型推导有疑问, 参见: 第三部分

第二点: 临时变量真的是const吗? 看下面:
template <typename _Type>
class _Temp
{
public:
    _Temp<_Type>& set( const _Type& value ){ this->__value = value; return *this; }
    const _Type& get() const {
        cout << "const _Type& get() const : " << __value << endl;
        return __value;
    }
    _Type& get() {
        cout << " _Type& get() : " << __value << endl;
        return __value;
    }
public:
    _Type __value;
};

_Temp<int>().set(100).get();
//gcc 4.7 : _Type& get() : 100
//可见,临时变量本身是不具备const属性的;
*/


//测试5: 具名右值引用;
int main()
{
    int &&_R_Reference = int(100);
    cout << _R_Reference << endl;
    DeductionType( _R_Reference );
    cout << _R_Reference << endl;
}
/*
分析: 这里也是一个右值引用,能编译通过, 而测试3就失败了呢?
关键是因为这里的实参是一个具名的右值引用,具名的右值引用和不具名右值引用有一个重要的区别:
这个具名的右值引用在函数调用之后还能访问到还能使用;

再看测试3导致编译失败的原因是编译器要保证程序不能对右值做无意义的修改,
那么对具名右值引用的修改显然是有意义的,那么对于测试3中不具名右值存在的问题,在具名右值中不会出现,所以允许非const左值引用具名右值,
基于此,具名右值在其生存周期内的作用域中会被当做一个左值看待,
因此,编译器在推导模板的时候,会将具名右值引用类型的参数推导为左值引用类型;

//扩展: 试想一下, _Type& + && => _Type&& 这个规则会发生什么问题??
能用到_Type& + && 这个折叠规则的时候实参应该一定是一个具名右值吧, 否则的话会编译失败的(测试3),
那么上面提到过, 具名右值是在函数调用之后还能继续被访问的,如果编译器在这种情况下不折叠为一个左值,而折叠为一个右值,
那么这样至少存在两个问题:
1. 如果存在一个以该类型的右值为参数的重载函数, 那么这个函数会被优先匹配,既然函数的形参已经被显式声明为一个右值引用,
那么潜在的含义就是在函数内部(可能)会对形参应用移动语义,如果实参是一个具名右值得花, 那函数调用结束后这个具名右值就被移动掉了(内存所有权转移),
那么外部的这个具名右值引用的名称所访问到的就是一个被移动掉的对象;
2. 如果不存在一个该类型的右值做为参数的重载函数, 那么模板参数就需要被编译器推导,
那么问题来了: 实参若为一个左值,那么编译器推导出函数形参为左值, 实参若为一个右值时编译器推导出函数形参为一个右值,
那么函数体该如何实现,是按照左值来实现,还是按照右值来实现(区别在于是否实现移动语义);
*/



//测试6: const具名右值引用;
int main()
{
    //In instantiation of 'void DeductionType(_Type&) [with _Type = const int]':|
    const int &&_R_Reference = int(100);
    cout << _R_Reference << endl;
    DeductionType( _R_Reference );
    cout << _R_Reference << endl;
    return 0;
}
/*
编译成功,应用引用折叠规则: _Type& + && => _Type&,所以模板形参被折叠为_Type&,
而且编译器会保留实参的cv限定,所以_Type被推导为const int;
这个主要是想说明,引用折叠并不会影响cv修饰符的推导, 无论对于右值还是左值;
*/


第二部分: 实参演绎类型 const _Type&
template <typename _Type>
void DeductionType( const _Type& __type ) { __type.Method(); }

//测试1. 左值;
int main()
{
    int n = 100;
    DeductionType( n );
    return 0;
}
//模板参数_Type被推导为int, 函数的完整形参为 const int&


//测试2. 带有cv修饰符的左值;
int main()
{
    const volatile int n = 100;
    DeductionType( n );
    return 0;
}
/**
In instantiation of 'void DeductionType(const _Type&) [with _Type = volatile int]':|
从编译器输出信息上可以看到, 显式为函数形参中的模板参数类型加上cv修饰符后,编译器在推导模板参数时会忽略形参中已声明的cv修饰符推导;

注意: 这个规则仅针对于非指针变量, 对于指针变量不是这样的,参见测试3;
*/

//测试3. 带有cv修饰符的指针类型;
int main()
{
    const char* str = "hello world";
    DeductionType( str );
    return 0;
}
/**
In instantiation of 'void DeductionType(const _Type&) [with _Type = const char*]':|
从编译器输出信息看, 它将_Type推导为const char*, 而实际上函数的完整形参是: const char* const& .
通过下面的实现来验证:
template <typename _Type>
void DeductionType( const _Type& __type ) {  __type = "HELLO WORLD"; }
此时会编译失败: error: assignment of read-only reference '__type'
从编译信息上看,编译器认为__type是一个只读的,确实是有一个const在修饰__type本身;
实际上自始至终它们两个const的语义都是不同的: 实参中const修饰的是指针str指向的目标, 而在函数模板参数声明中的const修饰的是模板参数自身,所以这种推导是完全合理的.
下面这个现象与这里的问题类似:
typedef char* pchar_t;
const pchar_t p = "hello world"; //const修饰的是pchar_t本身.
p[0] = 'H'; //OK
p = "HELLO WORLD"; //编译失败.

基于此,在对一个带有const模板参数的函数模板进行对指针特例化时,要注意保留const:
template <typename _Type>
int _Find( const _Type* array, size_t size, const _Type& __search_value )
{
    //...
}

template <>
int _Find( const char** array, size_t size, const char*& __search_value )
{
    //...
}
上面这个特例化的函数模板是编译不过的:
error: template-id '_Find<>' for 'int _Find(const char**, size_t, const char*&)' does not match any template declaration|
提示这个特例化不能匹配任何模板定义, 原因就上面已经解释过;
正确的特例化形式为:
template <>
int _Find( const char* const* array, size_t size, const char* const& __search_value )
{
    //...
}
*/


//测试4: 不具名右值;
int main()
{
    DeductionType( 100 );
    DeductionType( int(100) );
    return 0;
}
/**
编译成功,应用引用折叠规则: _Type& + && => _Type&, 完整的模板函数实参为const int&, _Type被匹配为int;
因为形参已经被显式的指定const限定,所以对右值无意义的修改是不会发生的(参见: 第一部分 测试3);

因此初始化表达式:
const int& __type = 100;
和
const int& __type = int(100);
都是可以编译成功的;

其实对于const限定的模板参数而言,是否是具名的右值是没有什么区别的(所以这里的测试4,测试5都一样).
因为const限定引用本身的语义就已经明确约定: 不会对引用目标做出任何修改.
*/

//测试5. 具名右值引用;
int main()
{
    int &&_R_Reference = int(100);
    cout << _R_Reference << endl;
    DeductionType( _R_Reference );
    cout << _R_Reference << endl;
}
//编译成功, 应用引用折叠规则:_Type& + && => _Type&, 完整的模板函数实参为const int&, _Type被匹配为int;


//测试6. const具名右值引用;
int main()
{
    //In instantiation of 'void DeductionType(_Type&) [with _Type = const int]':|
    const int &&_R_Reference = int(100);
    cout << _R_Reference << endl;
    DeductionType( _R_Reference );
    cout << _R_Reference << endl;
    return 0;
}
/**
编译成功, 应用引用折叠规则:_Type& + && => _Type&, 完整的模板函数实参为const int&, _Type被匹配为int;
*/


第三部分: 实参演绎类型 _Type&&

#include <iostream>
using namespace std;
template <typename _Type>
void DeductionType( _Type&& __type ) { __type.Method(); }
//在函数参数声明中, 参数被声明为非const右值引用, 这向函数调用者传递了一个信息:
//在函数内部(可能)会使用移动语义,这意味着函数调用结束后,形参的内存所有权可能会被转移;


//测试1: 左值;
int main()
{
    int n = 100;
    DeductionType( n );

    return 0;
}
/**
In instantiation of 'void DeductionType(_Type&&) [with _Type = int&]':|
从编译器输出信息上看, 模板参数竟然被推导为一个实参类型的左值引用,而不是实参的基本类型(即int);
(后面有分析)
*/

//测试2:带有cv修饰符的左值;
int main()
{
    const int n = 100;
    DeductionType( n );
    return 0;
}

/**
In instantiation of 'void DeductionType(_Type&&) [with _Type = const int&]':|
编译器除了会保留cv限定之外, 对模板参数进行推导的结果与实参为左值的推导结果是一样的,也是实参类型的左值引用;
(后面有分析)
*/

//测试3: 具名右值;
int main()
{
    int &&_R_Reference = int(100);
    cout << _R_Reference << endl;
    DeductionType( _R_Reference );
    cout << _R_Reference << endl;
}
/**
In instantiation of 'void DeductionType(_Type&&) [with _Type = int&]':|
具名右值引用同样被推导为实参类型的左值引用,而不是基本的实参类型(即int);
(后面有分析)
*/

/**
分析: 测试1, 测试2, 测试3中, 编译器将模板参数都推导为一个实参类型的左值引用;
先看三种测试中的实参类型, 分别是左值, const左值, 具名右值, 他们共同的一个特点是在函数调用结束后都还可以被继续访问;
而前面说过, 函数的参数声明为非const右值引用,则意味着这(可能)会对参数使用移动语义(转移内存所有权),
那么对于这三种测试中的类型因为需要在函数调用完毕之后是可以继续合法访问的,所以不能对它们使用移动语义,因此,不能将函数模板形参推导为一个右值引用(潜在的意思是不能将模板参数_Type直接按照实参原始类型(int,const int)推导,因为这样的话函数形参的完整类型就是int&&, const int&&, int&&, 这将违背刚刚分析的这些);

那到这里我们能想到有两种选择来解决这个问题:
1. 继续将函数模板推导为一个右值引用,这样在初始化的时候,如果实参是一个左值(或具名右值)的话,就会导致初始化失败的编译错误(非const右值引用不能引用左值),
    同样避免了对上述三种类型使用移动语义;
2. 将上述三种类型都推导为左值,然后通过引用折叠规则来折叠为左值来避免这个问题;

而在C++的标准实现中,我们看到,使用的是第二种解决方案(但是对于const右值引用的模板形参推导时使用的是第一种方案,参见: 第四部分);
第二种相比第一种有什么好处呢:

其实应该都是差不多的(或许第一种是更标准的解决方法),但是第二种方式的好处是能够实现所谓的"完美转发";
(在其他文章中第二种方式被称为"模板参数推导的特殊原则",这种形参类型被称为"Universal Reference"或者"forwarding reference");
*/

//测试4: 不具名右值;
int main()
{
    DeductionType( 100 );
    DeductionType( int(100) );
    return 0;
}
/**
应用折叠规则: _Type&& + _Type&& => _Type&&, _Type被推导为int类型,而函数形参的完整声明为int&&;
但是这里要注意: 实参类型虽然是一个不具名右值,但是调用时实参向形参传递的过程中, 实际上等价于下面的表达式:
int&& __type = 100;
int&& __type = int(100);
发现了什么,不具名右值变成了一个具名右值,
那么这里就有一个很重要的问题: 在函数体内形参实际上是一个具名右值(也就是左值),这在实现完美转发的时候会有影响,这也就是完美转发必须配合forward的原因;
这篇文章主讲实参演绎(模板类型推导)这里就不解释forward了, 详细讲解参见: [STL源码导读 - move.h];
*/


第四部分: 实参演绎类型 const _Type&&
注: 这种形参声明暂无用途,仅作为扩展.
#include <iostream>
using namespace std;
template <typename _Type>
void DeductionType( const _Type&& __type ) { __type.Method(); }

测试1: 左值和带有cv限定的左值;
int main()
{
    int n = 100;
    DeductionType( n ); //error: cannot bind 'int' lvalue to 'const int&&'
    const int m = 100;
    DeductionType( m ); //error: cannot bind 'const int' lvalue to 'const int&&'
    return 0;
}

//测试2: 具名右值;
int main()
{
    int &&_R_Reference = int(100);
    cout << _R_Reference << endl;
    DeductionType( _R_Reference ); //error: cannot bind 'int' lvalue to 'const int&&'
    cout << _R_Reference << endl;
}
//注: 可以看到, 具名右值被编译器认为是左值.

/**
    测试1, 测试2都将编译错误,而错误原因就是不能用一个左值(具名右值)去初始化一个const右值;
    对于测试1的第一次调用,类似于下面的表达式:
    int v = 100;
    const int&& __n = v;
    测试1的第二次调用,类似于下面的表达式:
    const int v = 100;
    const int&& __m = v;
    测试2的调用类似于下面的表达式:
    int &&m = 100;
    const int&& n = m;

    想不通上面的这些表达式有什么不合理的地方,标准竟然不让编译通过,
    不过在<深入理解C++11 - C++11新特性解析与应用>这本书的第76页有一段话说:
    "通常情况下, 右值引用是不能够绑定到任何左值的",
     而这里的情况就是通过一个const右值引用来绑定到左值, 也许就是和这个规定有关吧？！.

    补充(参照第三部分的前三个例子的解释来理解):
    这里你可能会有一个疑问: 第三部分中的前三个例子中对模板参数推导为一个左值引用,然后发生折叠.
    但这里的测试1和测试2中模板参数却没有被推导为左值引用,原因就是:
    第三部分中的函数模板声明表现出了"函数内可能进行移动语义"这个含义,
    而函数实参不允许发生这种语义,所以为了避免这个矛盾,将模板参数推导成左值引用就是解决方法之一.
    而这里的函数形参声明中加上了const, 所以函数声明表现出了"函数内不会进行移动语义"的含义,
    所以,这里的演绎过程就变成正常情况(也就是说第三部分前三个例子中对模板参数推导成引用并不是正常情况);
*/

//测试3: 不具名右值;
int main()
{
    DeductionType( 100 );
    DeductionType( int(100) );
    return 0;
}
/**
    模板参数_Type被推导为int, 函数的形参完整声明为: const int&&;
    编译也可以成功.
*/


第五部分: 实参演绎类型 _Type
#include <iostream>
using namespace std;
template <typename _Type>
void DeductionType( _Type __type ) { __type.Method(); }
/*
对于一个函数我们将其形参声明为非引用,非指针,非带有cv限定符的类型, 那这个函数的声明表达的语义不就是值传递吗;
对于一个值传递的函数,形参无论是引用,指针,还是const,还是 volatile, 那么对于形参而言都没有意义,完全没有,因为它不需要保留实参的任何修饰;
就好比: int value = XXX; 对于value而言,它是不会在意XXX是右值还是左值, 是const还是volatile,是字面量或者临时变量的;
因此, 通过任何实参类型来实例化这个模板参数,推导后的_Type只是实参本身的基本类型而已(丢弃掉实参的任何限定和引用属性);
*/

int main()
{
    int __value1 = 100;
    const volatile int __value2 = 100;
    int& __value3 = __value1;
    int && __value4 = int(300);

    //DeductionType( __value1 ); //int
    //DeductionType( __value2 ); //int
    //DeductionType( __value3 ); //int
    //DeductionType( __value4 ); //int
    //DeductionType( 400 ); //int
}


### 引用折叠的基本依据

[8.3.2 References]/4
6 If a typedef (7.1.3), a type template-parameter (14.3.1), or a decltype-specifier (7.1.6.2) denotes a type TR that is a reference to a type T, an attempt to create the type “lvalue reference to cv TR” creates the type “lvalue reference to T”, while an attempt to create the type “rvalue reference to cv TR” creates the type TR.


### 扩展
+ https://isocpp.org/blog/2012/11/universal-references-in-c11-scott-meyers
+ https://en.cppreference.com/w/cpp/language/reference
+ https://stackoverflow.com/questions/39552272/is-there-a-difference-between-universal-references-and-forwarding-references
+ https://stackoverflow.com/questions/13725747/concise-explanation-of-reference-collapsing-rules-requested-1-a-a-2
+ <深入理解C++11 - C++11新特性解析与应用>/3.3
+ <C++ Templates>/11章
+ <C++必知必会>/条款57
-->
