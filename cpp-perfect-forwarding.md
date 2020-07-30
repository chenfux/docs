# 通用引用(universal References)与完美转发(Perfect forwarding)

文档迁移中...

<!--
//std::forward.

这个模板大多数都叫做"完美转发",这个名字挺抽象的, 其实这个模板的做的事情是:
在一个函数内部(暂且不考虑作为unevaluated operand的情况), 将函数的形参类型"还原"成实参类型,
将还原得到的结果如果再做为另一个内部函数的调用实参的话, 就相当于以调用外部函数的实参类型来调用这个内部函数, 这样看起来就像是在转发参数, 又由于转发的参数类型与传递给外部的函数类型完全一样, 所以称为完美转发.

注：这个"还原"操作主要是针对具名右值引用还原到右值引用,
    其余情况下,实参和形参类型都是一致的,还原之前和之后的结果都是一样的.


//1. 不使用std::forward的思路的话
, 这个功能实现起来并不简单:

(1)
template <typename T>
void shell( T v ) { todo(v); }
//无论调用shell时指定的实参是何种类型, 形参v都是对该类型的一个临时对象的拷贝,所以传递到todo的时候也不是原始类型.

(2)
template <typename T>
void shell( const T& v ){ todo(v); }
//如果todo没有声明一个接收"const左值类型"为参数的版本,则这个转发失败.
//同时不能转发右值.

(3)
template <typename T>
void shell( T& v ) { todo(v); }
//不能转发右值.

(4)
template <typename T>
void shell( T&& t ) { todo(v); }
//不能转发右值(因为在shell的函数体内变量t是一个具名右值,也就是一个左值);

而C++11引入了一个称为"引用折叠(reference collapsing)"的语法规则,配合右值引用,使得完美转发的实现变得简单.
首先,引用折叠的规则如下:


那我们可以将转函数实现成下面的样子:
template <typename T>
void shell( T && t )
{
    todo(static_cast<T &&>(t));
}

这个实现中,有两处会被编译器考虑引用折叠规则:
    1. 函数形参推导
    2. static_cast
根据这两点,
(1) 当我们向shell传递的实参一个是X类型的左值, 那么模板参数T将会被推导为左值引用类型,即X&
那函数形参声明就变成"X& &&", 再通过折叠规则, 最终形参被确定为X&.
同样, static_cast就会变成 static_cast<X& &&>(t),
再通过引用折叠规则变成 static_cast<X&>(t), 这个转换之后的结果就会被传递到todo函数中,
而这个转换之后的类型与调用shell的实参类型是完全一致的,所以这就实现了转发左值.
(2) 当我们向shell传递的实参是一个X类型的右值, 那么模板参数T将会被推导为右值引用类型,即X&&
那函数的形参声明就变成"T&& &&", 再通过折叠规则, 最终形参被确定为X&&.
同样, static_cast就会变成static_cast<X&& &&>(t),
再通过引用折叠规则就变成static_cast<T&&>(t);
最终这个转换的结果就是一个X&&右值,与传递给shell的实参类型是一致的,这样就实现了转发右值.

注:推导过程中函数实参的cv限定是保留到模板参数中的.


//2. std::forward;
上面的完美转发我们是通过一个static_cast来实现的, 实际上C++标准库已经提供了相同作用的函数模板, 就是std::forward.

template <typename T>
void shell( T && t )
{
    //利用这个函数模板,我们可以和使用static_cast一样实现完美转发(下面两行是等价的).
    todo(std::forward<T>(t));
    //todo(std::forward<T&&>(t));
}


//3. forward的实现
下面就是SGI-STL的forward实现(bits/move.h), 它有两个重载版本:
(1)
template<typename _Tp>
constexpr _Tp&& forward(typename std::remove_reference<_Tp>::type& __t) noexcept
{ return static_cast<_Tp&&>(__t); }

(2)
template<typename _Tp>
constexpr _Tp&& forward(typename std::remove_reference<_Tp>::type&& __t) noexcept
{
 static_assert(!std::is_lvalue_reference<_Tp>::value, "template argument"
 " substituting _Tp is an lvalue reference type");
 return static_cast<_Tp&&>(__t);
}

第(1)个重载版本会接收
具名右值实参并以右值引用类型返回, 也接收左值实参依然返回左值引用类型.
第(2)个重载版本只接收右值实参并以对应的右值引用类型返回.


关于具体实现, C++标准文档已经明确规定了:
1 The library provides templated helper functions to simplify applying move semantics to an lvalue and to
simplify the implementation of forwarding functions.
template <class T> T&& forward(typename remove_reference<T>::type& t) noexcept;
template <class T> T&& forward(typename remove_reference<T>::type&& t) noexcept;
2 Returns: static_cast<T&&>(t).
3 if the second form is instantiated with an lvalue reference type, the program is ill-formed.

第一点规定了函数模板声明,
第二点规定了函数模板实现,
第三点规定了第(2)个重载版本的实现要注意的问题.

下面呢,以我个人的见解在详细解释两点:
1. 关于函数模板的声明, 2. 关于上述标准规定的第三点有什么含义或语义.

1. 可以看到forward的函数模板声明是这样的:
  typename std::remove_reference<_Tp>::type&
  typename std::remove_reference<_Tp>::type&&
    其中typename用来指出remove_reference<_Tp>::type是一个类型(参见[模板-依赖型名称]);
    而结合remove_reference的实现,你会发现这个形参的声明最后会变成下面的样子:
    forward( _Tp& __t);
    forward( _Tp&& __t);
    那这里就会有一个问题:
        像上面这样先删除掉引用部分再明确的添加上引用符号 与 直接声明为_Tp&/_Tp&&有什么区别呢?

根据我的理解, 这两个声明的形参类型声明虽然看起来是一样的,
但前者<禁止了编译器对模板参数类型的自动推导>,这要求我们在使用forward的时候必须明确指定一个模板参数类型,
所以在这个声明之下其forward的形参类型是确定的,就是_Tp&和_Tp&&不会被改变的.
而后者则会被编译器根据实参类型自动推导模板参数来确定形参类型, 这可能发生引用折叠,所以相对于前者而言,后者的形参类型声明是可能会被折叠而导致发生改变的(_Tp&&可能会被折叠成_Tp&,这就导致std::forward两个重载版本的声明完全相同).

注: 关于上述<禁止了编译器对模板参数类型的自动推导>这个说法的依据是<C++编译器不能推导一个受限的类型名称>这个前提. [参见 C++ Templates 11.2]

2.
if the second form is instantiated with an lvalue reference type, the program is ill-formed.
这里意思是说: 如果第(2)个重载形式是通过一个左值引用类型来实例化的话,这个程序是不符合语法的.
因此你会看到实现的时候第(2)个重载形式对这种情况进行了静态断言.

其实这个限制也不难理解, 一般使用这个重载形式的时候可能会像下面这样:

template <typename T>
void shell( T && t )
{
    todo(std::forward<T>(std::move(t)));     //ok
    //todo(std::forward<T&&>(std::move(t))); //ok
    //todo(std::forward<T&>(std::move(t))); //语义冲突.
}

这种情况下, 匹配的就是forward的第(2)种重载形式.
在这种情况下, 我们明确使用了std::move这个函数,那么就表达的含义就是: 我们明确表示想要在todo函数中使用移动语义.
同时,如果我们显式的将forward的模板参数指定为T或T&&,则与这个我们所期待的并无冲突,最终也能实现我们的目的.
然而,如果我们显式的将forward的模板参数指定为T&, 那么forward的实现中 static_cast<T &&>(__t)这一个表达式就会变成static_cast<T& &&>(__t),从而导致结果是一个左值引用类型,与我们的期待是冲突的.
因此,我想这条标准的含义就是为了避免这个冲突吧。


### 扩展

深入理解c++11 - c++11新特性解析与应用 3.3.6
http://www.cppblog.com/kesalin/archive/2009/06/05/86851.html -->
