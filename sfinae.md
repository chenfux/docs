<!--
{
	"title": "SFINAE(Substitution Failure Is Not An Error) 替换失败不是错误",
	"href": "http://www.idioms.cc/page/sfinae.html",
	"date": "2018-10-20"
}
-->

# SFINAE(Substitution Failure Is Not An Error) 替换失败不是错误

> 除特别说明, 全文默认依据 ISO C++ 11 标准. 工作草案[N3337](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3337.pdf)包含ISO C++11标准和一些修改.

### 起源

"substitution-failure-is-not-an-error(SFINAE)"原则最初是在<C++ Templates: The Complete Guide>这本书中提出<sup>[1][2]</sup>, "SFINAE"在书中首次提到的上下文[8.3.1]中描述了这样一个行为:

现有两个重载的函数模板,

```
template<typename T> RT1 test(typename T::X const*);
template<typename T> RT2 test(...); 
```
 
由于模板参数是可以显式指定的, 所以引用`test`时指定的模板实参可能导致`typename T::X const*`产生一个无效的类型, 比如 `test<int>`, 这里的`int`替换掉`T`后导致第一个版本的形参类型变成`typename int::X const*`, 这是一个错误的C++类型, 但是第二个重载版本却是正确的(这个前提很重要), 这种情况, `test<int>` 这个表达式将不会被视为编译错误.

作者将这整个过程总结为"substitution-failure-is-not-an-error"(SFINAE)这样一个原则, "substitution"指的就是模板实参对形参的替换, "failure"指的就是替换失败产生无效的C++类型, is-not-an-error指的就是引发这个替换的表达式不被视为编译错误(而是考虑其他的重载版本), 这就是SFINAE原则的由来.

从这个过程中可以看到, SFINAE原则体现的是一种"替换失败即忽略"的模板替换错误处理思路, 因此这也使SFINAE原则成为了一种通过排除法来选择重载函数模板的(重载解析)机制, 所以作者说它是使得函数模板重载得以实现的重要因素. 函数模板重载构成了一种[ad-hoc多态](https://zh.wikipedia.org/wiki/%E7%89%B9%E8%AE%BE%E5%A4%9A%E6%80%81), 而利用SFINAE原则可以显式控制(排除/选择)需要参与ad-hoc多态的函数模板版本, 这是C++编译时编程能力强大的重要一点.

另外, 可以看到, SFINAE原则最初提出是仅针对重载的函数模板, 而此后, 这个原则也被用来描述对类模板特化的匹配行为(这一点可以从标准委员会工作文件/论文以及libstdc++的注释中发现).

> 1. https://accu.org/content/conf2013/Jonathan_Wakely_sfinae.pdf
> 2. https://en.wikipedia.org/wiki/Substitution_failure_is_not_an_error

### 利用SFINAE原则编程

SFINAE原则本身并没有表明它应该如何应用到编程中, 但是既然它被总结为一种特定于模板参数替换失败的场景下发生的行为, 那么只需要构建一种模板参数替换失败的场景, SFINAE原则自然会被应用. 由于模板参数替换失败的场景取决于不同目的, 所以这里就假设"以检测某个类型T是否为int类型"这个目的来说明如何利用SFINAE原则编程:

提供两个名为`is_int`的重载函数模板, 两个重载分别具有不同的目的, 
	其中一个重载用于测试`T`是否为`int`类型, 如果不是的话想办法让这个版本替换出错,
	另一个重载用作备胎版本, 保证对任意模板参数类型`T`都是well-formed,

这里将备胎版本声明如下: 

```cpp
template <typename T> static std::false is_int(...);
```

在这个声明中, 形参类型为`...`, 这意味着它可以接收任何类型实参[todo-link], 并且没有使用模板参数`T`, 所以无论模板参数`T`是什么类型, 这个声明都是正确的. 备胎版本被选中的条件是SFINAE原则已经被应用, 此时测试版本的函数模板已经被忽略, 这意味着测试失败, 即`T`不是`int`类型, 所以备胎版本的返回值为std::false.

对测试版本的声明如下:

```cpp
template <bool>
struct eif_ {};

template <>
struct eif_<true>  { typedef void type;	};

template <typename T> static std::true_type is_int(typename efi_<std::is_same<T, int>::value>::type*);
```

这个声明用于测试`T`是否为`int`, 其思路就是判断`eif_::type`是否是有效的C++类型, 当`T`为`int`时, `std::is_same<T, int>::value`为true, 此时`eif_<true>::type`是有效的C++类型, 此模板将被用来产生函数模板特化(备胎版本也会产生一个函数模板特化), 并且由于`0`对`typename efi_<std::is_same<T, int>::value>::type*`的匹配程度高于`...`, 所以该模板产生的函数模板特化被调用. 否则, `eif_<false>::type`将是无效的C++类型并导致SFINAE原则被应用, 此重载版本被忽略, 转而选择备胎版本.

之后可以像这样引用模板:

```cpp
cout << decltype(is_int<int>(0))::value << std::endl;
cout << decltype(is_int<double>(0))::value << std::endl;
```

总的来说, 利用SFINAE原则编程关键是要制造一种模板参数替换失败的场景, 而模板参数替换失败的场景指的是替换产生无效的C++类型(或表达式) [14.8.2/8]. 另外就是要提供一个备胎版本, 用于当测试版本失败时的备选, 否则的话会导致真正的编译错误.

### SFINAE 工作原理

SFINAE原则是从一种现象中总结出来, 它所描述的行为本身就是已经存在着的, 并且有着一系列明确的标准共同构成的这种行为.
下面以"检测类T是否存在一个名为walk并且类型为void(T::\*)()的成员函数"为例描述SFINAE原则的工作原理.

```cpp
template <typename T>
struct has_member_function_walk
{
    template <typename U, void(U::*)()>
    struct test_helper;

    template <typename U>
    static std::true_type test(test_helper<U, &U::walk> *); // #1

    template <typename U>
    static std::false_type test(...); // #2

    static constexpr bool value = decltype(test<T>(0))::value;
};
```

+ 首先由`test<T>(0)`引发对所有`test`函数模板执行模板参数推导(Template argument deduction),
	模板参数推导的目的是要为函数模板的每个模板参数找相应的模板参数值, 并用这些模板参数值来实例化这个函数模板以产生可以被真正调用的函数模板特化 [13.3.1/7 | 14.8.3/1],

	注: 函数模板特化(function template specialization), 是指从函数模板实例化出来的函数 [14.8/1]. 在这里要明确一点: 函数模板不是函数, 并不会直接参与重载解析, 也不能真正的被调用, 只有当函数模板被实例化为相应的函数模板特化时才是真正的函数, 才能参与重载解析. 所以这里涉及的几个概念的关系是: 函数模板被调用 -> 函数模板被(显式)实例化 -> 模板参数推导 -> 产生函数模板特化 -> 重载解析.

+ 模板参数推导并不仅仅是推导模板参数, 还包括对显式指定的模板参数替换和对被推导出来的模板参数替换这两个替换阶段.

+ 其中, 显式指定的模板参数替换发生在推导模板参数之前, 将所有显式指定的模板参数(实参)替换掉函数类型和模板形参声明中(不包括函数体)出现的对应模板形参, 这个过程就是第一次替换阶段, 也就是SFINAE原则中的"S" [14.8.2/5 | 14.8.2/6 | 14.8.2/7]. 所以对#1执行替换后产生函数类型`std::true_type test(test_helper<T, &T::walk> *)` [14.8.2/2],

+ 如果替换后的表达式`&T::walk`是ill-formed, 则这意味着这次替换产生了无效的表达式, 这被视为对#1执行模板参数推导(过程)失败(deduction fails, F) [14.8.2/8], 这就是由"错误(ill-formed)"到"失败(failure)"的转折点,

+ 模板参数推导失败不会导致编译错误(INAE), 但后果是这个函数模板不会被实例化, 所以#1不会有相应的"函数模板特化"被加入到`test`的候选函数集合 [14.8.3/1],

+ 而#2则不会推导失败, 所以一个相应的函数模板特化被加入到`__test`候选函数集合 [13.3.1/7 | 14.8.3/1],

+ 当对`__test(0)`执行重载解析时, 候选函数集合中只有#2的函数模板特化, 并且匹配成功, 所以最终计算返回值类型为std::false_type,

+ 相反, 如果替换后的表达式`&T::walk`是well-formed, 则说明T存在名为walk的成员函数,

+ 由于`&T::walk`在此处是作为`test_helper`的非类型模板实参, 所以模板参数检查将发生在`&T::walk`这个表达式上, 其中包括检查形参与实参类型是否匹配, 即`&T::walk`的类型是否匹配`void(C::*)()`, 如果匹配失败将导致程序ill-formed [14.8.2/2 | 14.3.2/5],

+ 如果对`&T::walk`的检查失败导致ill-formed, 则`test_helper<T, &T::walk> *`是一个无效的类型, 这同样被视为对#1模板参数推导失败(deduction fails, F) [14.8.2/8], 后续同上,

+ 如果对`&T::walk`的检查成功, 接下来将对被替换后的函数类型进行一些函数形参类型调整, 比如: `void ()(const int, int[5])` 调整为 `void(*)(int,int*)`, 但是这里不需要这样的调整.  [14.8.2/5] 

+ 在替换了显式指定的模板参数和调整了函数形参类型后, 如果还有模板参数没有被替换, 那这些模板参数就要通过推导模板参数来获得, 如果无法通过模板参数获得, 则使用默认模板参数. 当所有的模板参数都已经被推导或通过默认值得到后, 进行第二次模板参数替换阶段. 如同第一次替换一样, 此过程替换失败同样视为模板参数推导失败.

+ 本例没有需要被推导的模板参数, 所以#1执行模板参数推导成功, 相应的函数模板特化被产生并加入`test`候选函数集合, 同时对#2执行模板参数推导也成功, 相应的函数模板特化也被加入到候选函数集合 [14.8.3/1],

+ 此时`test(0)`的候选函数集合有两个声明, 但`test(0)`执行重载解析时, 对#1的函数模板特化比对#2的函数模板特化更匹配, 所以最终计算返回值类型为std::true_type.


### 扩展

 + http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2006/n1978.pdf [2.6 Decltype and SFINAE]
 + https://msdn.microsoft.com/zh-cn/library/s016dfe8.aspx
 + https://en.cppreference.com/w/cpp/language/overload_resolution
 + http://www.catonmat.net/blog/cpp-polymorphism/
 + https://en.wikipedia.org/wiki/Ad_hoc_polymorphism  
 + https://en.wikipedia.org/wiki/Polymorphism_(computer_science)
 + https://en.wikipedia.org/wiki/Parametric_polymorphism
 + http://lucacardelli.name/Papers/OnUnderstanding.pdf
 + https://cs.stackexchange.com/questions/82485/is-subtype-polymorphism-a-kind-of-ad-hoc-polymorphism
 + https://stackoverflow.com/questions/5854581/polymorphism-in-c
 + https://en.cppreference.com/w/cpp/language/sfinae

