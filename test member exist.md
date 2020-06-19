# 根据名称检测类成员是否存在

有时, 识别类中是否存在某个特定名称的类型成员, 成员函数, 数据成员都可能被作为对类型特化处理的条件,
但是目前C++标准库并没有直接提供能够识别这些信息的traits. 作为补充, 这里演示多种实现, 但本质上全都依赖于[SFINAE(Substitution Failure Is Not An Error)]原则(/page/sfinae.md).

### SFINAE原则

http://www.idioms.cc/page/sfinae.html

详细描述了SFINAE原则的起源, 使用和原理, 这里不再详述. 

### 根据名称检测成员函数是否存在

以"检测类T是否存在一个名为`walk`并且类型为`void(T::*)()`的成员函数"为例, 演示各种实现.

> 除特别说明, 全文默认依据 ISO C++ 11 标准. 工作草案[N3337](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3337.pdf)包含ISO C++11标准和一些修改.

---

第一种实现是分两步检测,

+ 第一步是检测T::walk是否存在. 这可以通过判断表达式`&T::walk`是否会导致程序ill-formed来确定, 如果不会, 则表示T::walk存在, 并继续第二步检测, 否则, 可能由于T::walk是私有的, 或者对T::walk的访问是歧义的[10.2], 或者T::walk真的不存在, 都会导致程序ill-formed, 而这些情况都统一视为T::walk不存在处理, 此时SFINAE原则被触发, 不需要进行第二步检测.

+ 第二步是检测T::walk的类型是否匹配. 当且仅当T::walk存在时, 这个检测才有意义. 检测方式是将`&T::walk`作为一个非类型模板实参来判断与对应的模板形参类型是否匹配, 如果不匹配, 这将导致程序ill-formed [14.3.2/5], 否则, 表示类型匹配. 

最后合并两部分的检测结果, 如果任何一步导致程序ill-formed, 则视为检测失败, 否则视为检测成功.

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

> 注: 不要试图使用`decltype(&T::walk)`这种方式来检测表达式, 因为`decltype`不能计算出被重载函数的地址, 所以如果walk被重载, 表达式`decltype(&T::walk)`将导致程序ill-formed, 这是不合理的 [7.1.6.2/4/-1].

---

第二种实现是尝试调用一下这个函数, 比如`T().walk()`, 如果函数调用表达式是well-formed, 则表示T::walk存在并且可以空参调用, 否则, 可能由于T::walk是私有的, 或者对T::walk的访问歧义[10.2], 或者T::walk真的不存在, 或者重载解析失败, 都会导致ill-formed表达式, 而这些情况都统一视为T::walk不存在处理.

这个实现的关键是要让这个函数调用表达式参与到模板参数推导中, 这样才能构建SFINAE原则的触发的场景. 但是这个函数调用表达式并不是一个常量表达式, 也不是类型, 所以它既没法作为被推导的函数模板的返回值类型, 也不能作为形参类型, 也不能作为模板(类型或非类型)形参默认值[14.3.2/1], 并且也没法借助类似于第一种思路中`test_helper`这样的辅助模板来构成一个类型(它不是常量表达式), 也就是说, 这个函数调用表达式仅靠自己无法参与到模板参数推导过程. 所以, 这里必须要借助`sizeof`, `noexcept`, `decltype`, `alignof`这几个操作符来让这个函数调用表达式变成常量表达式或类型. 其中,

 + `sizeof`操作符用于计算其操作数的对象表示[3.9/4]所占字节数的大小, 其结果为std::size_t类型常量[5.3.3/6], 因此, `sizeof(T().walk())`将变成一个表示整数值的编译时常量表达式, 这时候再借助辅助模板就可以构造出一个类型或者直接作为非类型模板参数默认值.

+ `noexcept`操作符用于确定其操作数是否声明为不抛出任何异常, 其结果为bool类型rvalue[5.3.7], 但就这里的目的而言, 无论noexcept操作符求值结果是true/false, 只要walk函数调用表达式是有效的表达式, 就证明空参的walk函数存在, 可以利用这一点, 将`noexcept(T().walk())`作为一个bool类型的常量表达式, 然后借助辅助模板就可以构造一个类型或直接作为非类型模板参数默认值.

+ `decltype`操作符用于查询其操作数的类型, 即, 表达式`decltype(e)`代表其查询到的操作数`e`的类型[7.1.6.2/4], 因此, `decltype(T().walk()) *`直接就构成一个类型(指向walk返回值类型的指针类型).

+ `alignof`计算操作数的对齐大小[5.3.6], 结果为std::size_t常量值, 所以使用上类似于`sizeof`.

另外关键的一点是要构造有效的对象表达式(object expression [5.2.5/4]). 像前面使用的`T().walk()`这个表达式是有问题的, 因为对象表达式`T()`是一个构造函数调用表达式, 而类型`T`可能不允许构造对象或不允许默认构造对象, 在这种情况下`T()`将是一个无效的表达式并且导致模板参数推导失败[14.8.2/8], 从而无法区分到底是对象表达式无效还是walk调用表达式无效. 由于这里的意图是仅对walk调用表达式进行测试, 所以前提是要保证对象表达式总是有效, 即使构造函数不可用. 另外, 上面已经解释过: 整个函数调用表达式都将被作为sizeof,decltype,noexcept,alignof这四个操作符的操作数, 而这四个操作符不会对操作数求值<sup>[1]</sup>, 这意味着作为操作数的函数调用表达式并不会真正的被调用, 根据这一前提, 这里有两种方法来产生有效的对象表达式:

+ 第一种是简单的将0向T\*强制类型转换然后解引用的表达式`(*(T*)0)`作为对象表达式, 以这种方式构造的对象表达式, 当T是引用类型时, 这个表达式是错误的(也就是说这这种方式也不完全总是有效的) [todo-link], 当T不是引用类型时,这个表达式将总是一个T类型的左值引用 [todo-link].
+ 还有一种是借助函数辅助的方式, 将`T`作为某个函数`f`的返回值类型(`T f();`), 然后利用函数调用表达式`f()`来作为对象表达式, 这种方式可以构造T&&和T&类型的对象表达式. (由于`f`不会被真正调用, 所以也不需要函数体<sup>2</sup>)

> [1] `alignof` 在C++11中并没有被明确为不求值上下文, 直到2018年5月份的工作草案[N4750]()都没有明确, 但是在提案[p0314r0](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0314r0.html)中被明确提出了标准变更细节, 不过不知道这份文件何时会反映到工作草案或标准中.(所以在此之前, 说alignof是不求值上下文应该不太稳妥).
>
> [2] 关于`f`的函数体总共就三种选择:
>
> 1. 如果给出其函数定义及返回值的话那总是要构造T类型的对象作为返回值, 但这又将面临T的构造函数能否调用的问题
> 2. 如果给出定义却不显式指定返回值的话(空函数体), 这属于未定义行为[6.6.3]
>	https://stackoverflow.com/questions/9936011/if-a-function-returns-no-value-with-a-valid-return-type-is-it-okay-to-for-the
> 3. 如果不给出定义的话, 程序well-formed, 但是在求值上下文中调用的话会导致链接失败.
>
> 显然, 最好的办法就是3., 然后仅不求值上下文才中调用`f()`.

> 引入术语"unevaluated operand"到C++11标准的说明文档:  
	http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2253.html  
> 引入`decltype`操作符和"返回值类型后置语法"到C++11标准的说明文档,  
	http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2006/n1978.pdf  

下面首先演示利用`sizeof`对walk调用表达式应用SFINAE.

```cpp
template <typename T>
struct has_member_function_walk {
	template <bool>
	struct __test_helper;

	template <typename U>
	static U f();

	template <typename U>
	static std::true_type __test(__test_helper<sizeof(f<U>().walk())>*);

	template <typename U>
	static std::false_type __test(...);

	static constexpr bool value = decltype(__test<T>(0))::value;
};
```

利用`sizeof`的实现有个很大的问题是, 当返回值类型为void时, `sizeof(f().walk())` 等价于 `sizeof(void)`, 而这是ill-formed, 因为sizeof明确规定其操作数不能是不完整类型(incomplete) [5.3.3/1], 而void就是不完整类型 [3.9.1/9].
下面再演示利用`noexcept`对walk调用表达式应用SFINAE.

```cpp
template <typename T>
struct has_member_function_walk {
	template <bool>
	struct __test_helper;

	template <typename U>
    static U f();

	template <typename U>
	static std::true_type __test(__test_helper<noexcept(f<U>().walk())>*);

	template <typename U>
	static std::false_type __test(...);

	static constexpr bool value = decltype(__test<T>(0))::value;
};
```

利用`noexcept`的实现最大问题是难以检测到返回值类型.
最后下面再演示利用`decltype`对walk调用表达式应用SFINAE

```cpp
template <typename T>
struct has_member_function_walk
{
    //static T f();

    template <typename U>
    static U f();

    template <typename U>
    static std::true_type __test(decltype(f<U>().walk())*);

    template <typename>
    static std::false_type __test(...);

    static constexpr bool value = decltype(__test<T>(0))::value;
};
```

> 注, 当前的实现中`__test_helper`必须是静态成员函数模板, 而不能只是一个静态成员函数.
  这是因为在对`has_member_function_walk`实例化的过程中, `T`的模板实参值是已知的, 类定义中所有使用`T`的地方都将被替换成对应的模板实参,
  当(所有? 每个?)模板实参替换完成后, 语法检查将被执行, 此时,
  如果`__test_helper()`不是函数模板, 则表达式`__test_helper().walk()`是否是well-formed在替换后就是已知的,
  因此如果这个表达式是ill-formed, 则意味着实例化过程中产生了错误的构造, 这将导致编译失败, 不会应用SFINAE规则(因为SFINAE应用的前提是在函数模板参数推导/替换过程中, 而此时的错误是发生在对类模板的实例化过程中).
  如果`__test_helper()`是函数模板, 则`__test_helper<U>().walk()`是否是ill-formed是未知的(因为替换还不会导致`__test`实例化),
  这要延迟到`__test`被实例化的时候才能知道(对value求值会导致`__test`被实例化),
  而`__test`的实例化过程即使`__test_helper<U>().walk()`导致程序ill-formed, 也会被应用SFINAE规则.

    注, 在g++ 4.8+中, 如果`T::walk`是私有的, 无论`__test_helper()`是不是函数模板, 都会编译通过,
    而clang++ (和g++ 4.7.x)中, 如果`T::walk`是私有的, 但`__test_helper()`不是函数模板, 则编译失败, 理论上来看, clang++是更符合标准.
    猜测这可能是g++ 4.8+的bug, 而且这个bug可能是修复另一个bug的时候改出来的, 参考:
    https://gcc.gnu.org/bugzilla/show_bug.cgi?id=51213
    http://releases.llvm.org/3.7.1/tools/docs/LanguageExtensions.html#checks-for-standard-language-features
    https://wg21.cmeerw.net/cwg/issue1170

  todo: 补充具体引用, 大致分布[14.7 | 14.7.1]


下面是一个利用`decltype`并且基于C++11后置返回类型语法(trailing-return-type)的另一种实现, 这个实现是利用"逗号表达式的结果是最后一个子表达式的结果"和"逗号表达式的求值顺序是明确的从左到右进行"这两个保证 [5.18/1], 这意味着能执行到最后一个子表达式就说明前面的表达式没有任何一个导致ill-formed.

> 注, 必须要"科普"的一个常识是: C/C++中不要想当然的认为表达式的求值顺序是从左到右, 很多情况下并没有明确顺序, 并且括号并不能改变求值顺序 (C++11),
    参考 [`evaluation order`](/page/evaluation-order.md).

```cpp
template <typename U>
static auto __test(int) -> decltype(ft<U>().walk(), std::true_type());
```

如果对返回值类型有要求, 则需要单独检测,

```cpp
template <typename U>
static typename std::is_same<int, decltype(ft<U>().walk())>::type __test(int);
```

在前面实现中, 为了得到T类型的值需要借助一个额外的`f`模板, 其实从c++11以后, 标准库中提供的`std::declval`就是用于满足类似的这种需求,
其中`std::declval`与前面实现的`f`主要有两点不同,

+ std::declval 以静态断言强制保证它只能被作为不求值操作数来使用.
+ std::declval 的返回值类型是 typename std::add_rvalue_reference<T>::type, 这样做可以保证... [待确认].

最后将`f`替换为std::declval实现,

```cpp
template <typename U>
static typename std::is_same<int, decltype(std::declval<U>().walk())>::type __test(int);
```

> 这种利用decltype来触发SFINAE的方式叫做 "expression SFINAE", 参考:
>
> http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2634.html  
> https://stackoverflow.com/questions/12654067/what-is-expression-sfinae  
> http://www.idioms.cc/page/sfinae.html  

> std::declval 被引入C++11标准的提案:
> http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2009/n2958.html#Value

---

第三种实现是通过类模板特化 + [std::void_t (c++17)](/page/void_t.md), 这是一种类模板的SFINAE原则应用.

```cpp
template <typename T, void(T::*)()>
struct has_member_function_walk_tester;

template <typename T, typename = std::__void_t<>>
class has_member_function_walk_helper
: public std::false_type {};

template <typename T>
class has_member_function_walk_helper<T, std::__void_t<has_member_function_walk_tester<T, &T::walk>>>
: public std::true_type {};

template <typename T>
struct has_member_function_walk
: public has_member_function_walk_helper<T>
{};
```

> 注, 上面的实现在g++(5.4.0)/clang++(3.8.0)不兼容.
> 如果T::walk是私有的, g++会编译失败:`error: ‘void T::walk()’ is private`, clang++编译通过.
> 在这里不太好定论哪个对错, 因为C++标准中对类模板特化的SFINAE行为没有明确描述,  
    http://www.open-std.org/jtc1/sc22/wg21/docs/cwg_active.html#2054  
    https://stackoverflow.com/questions/30676839/sfinae-and-partial-class-template-specializations
>
>但是(瞎猜), 如果以函数模板的标准来看待类模板特化的话, 访问检查会被作为类模板的模板参数替换过程的一部分, 此时如果由替换产生访问检查失败(即替换失败)应该被视为"SFINAE错误"而不是ill-formed,
另一方面从目前的现象来看, 很可能是g++并没有将访问检查会被作为类模板的模板参数替换过程的一部分, 也就是说模板参数替换过程不进行访问检查,当后续实例化类模板的时候再进行访问检查, 巧的是, 这个行为正是c++11标准正式发布前的草案明确定义的行为(标准正式发布前已修改),
但到底是不是由于g++还未来得及跟进修正而导致这个问题, 不好确定.  
    https://wg21.cmeerw.net/cwg/issue1170  
    http://www.open-std.org/jtc1/sc22/wg21/docs/cwg_defects.html#1170  


使用walk函数调用表达式来作为测试表达式可以避免上面实现中出现的不兼容问题, 但缺点是没有同时检测返回值类型,

```cpp
template <typename T>
class has_member_walk_helper<T, std::__void_t<decltype(std::declval<T>().walk())>>
: public std::true_type {};
```

最后对上一个实现增加返回值类型检查,

```cpp
template <typename T>
class has_member_walk_helper<T, std::__void_t<decltype(std::declval<T>().walk())>>
: public std::true_type {};
```

> 注, std::void_t 是C++17才进入标准库, 所以在此之前, 可以使用非标准的std::\_\_void\_t (libstdc++, libc++ 都支持), 也可以自己实现一个, 参考 [todo-link].

---

还有一种比较奇特的实现思路有必要了解一下, 这个思路的实现也要将整个检测分成两步, 第一步是判断walk成员是否存在, 其基本思想是通过制造名称查找歧义产生ill-formed来触发SFINAE, 第二步是判断walk成员的类型是否匹配.

> 来源: https://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Member_Detector

首先是定义一个用于制造歧义的类B, 并在类B中先定义一个名为walk的成员函数,

```cpp
struct B
{
    void walk()
    {

    };
};

template <typename T>
struct D : T, B
{

};
```

之后再定义一个类D, 并同时继承要被检测的类T和B两个类. 这种继承关系下, 如果T中存在一个名为walk的成员函数, 则将导致D继承到分别来自B和T的两个名为walk的成员函数, 否则, D将只继承一个来自B的walk成员函数,
那么就可以利用对D::walk的访问是否会因为名称查找而歧义导致程序ill-formed来确定T中是否存在名为walk的成员函数.

> 注, 继承的访问控制并不会影响名称查找集合, 因为访问控制规则是在名称查找之后应用的[3.4/1 | 10.2/1], 也就是说, 即使T是私有继承, 名称查找也能找到它的walk成员(如果有),
    所以名称查找阶段如果在D中发现两个分别来自于不同基类的walk成员就会导致程序ill-formed [10.2/1 | 10.2/6/-2 | 10.2/13].

下面是这个思路的第一步检测的实现, 可以确定walk是否存在于T(私有成员也能被检测到).

```cpp
template <typename T>
struct has_member_function_walk
{
    struct B { void walk() {} };
    struct D : T, B {};

    template <void(B::*)()>
    struct __test_helper;

    template <typename U>
    static std::false_type __test(__test_helper<&U::walk> *);   //没有歧义则意味着T::walk不存在, 所以这里是返回std::false.

    template <typename>
    static std::true_type __test(...);

    static constexpr bool value = decltype(__test<D>(0))::value;
};

```

第二步, 确定walk成员类型是否匹配(私有成员被忽略),

```cpp
#include <iostream>
#include <type_traits>

template <typename T, typename R, typename... Args>
struct has_member_function_walk
{
        struct B { void walk() {} };
        struct D : T, B {};

        template <void(B::*)()>
        struct __test_helper;

        template <typename U, R(U::*)(Args...) = &U::walk>
        static std::true_type __test(int);

        template <typename>
        static std::false_type __test(...);

        template <typename U>
        static std::false_type __test2(__test_helper<&U::walk> *);

        template <typename>
        static decltype(__test<T>(0)) __test2(...);

        static constexpr bool value = decltype(__test2<D>(0))::value;
};
```

这个思路总的来说实现起来比前面的思路都要麻烦, 也并没有明显优势, 所以并不太实用, 但是这种思路我觉得还是可以了解下的.

### 根据名称检测数据成员是否存在.

以检测类T中是否存在一个名为value的bool类型数据成员为例.

---

前面针对成员函数检测的所有方式(对成员函数调用的方式需要变成对数据成员的访问)都可以用于对数据成员进行检测, 另外, 成员函数检测不能不用的`decltype(T::value)`表达式在这里也可以用(因为数据成员不存在重载, 所以不会遇到对成员函数检测时所面临的问题), 例如,

```cpp
template <typename T>
struct has_data_member_value
{
    template <typename U, bool U::*>
    struct __test_helper;

    template <typename U>
    static std::true_type __test(__test_helper<U, &U::value> *);

    template <typename U>
    static std::false_type __test(...);

    static constexpr bool value = decltype(__test<T>(0))::value;
};
```

```cpp
template <typename T>
struct has_data_member_value
{

    template <typename U>
    static std::is_same<bool, decltype(std::declval<U>().value)> __test(int);

    template <typename>
    static std::false_type __test(...);

    static constexpr bool value = decltype(__test<T>(0))::value;
};
```

```cpp
template <typename T, bool T::*>
struct has_data_member_value_tester;

template <typename T, typename = std::__void_t<>>
class has_data_member_value_helper
: public std::false_type {};

template <typename T>
class has_data_member_value_helper<T, std::__void_t<has_data_member_value_tester<T, &T::value>>>
: public std::true_type {};

template <typename T>
struct has_data_member_value
: public has_data_member_value_helper<T> {};
```

> 仅限clang++

```cpp
template <typename T>
struct has_data_member_value
{

    template <typename U>
    static std::is_same<bool U::*, decltype(&U::value)>  __test(int);

    template <typename>
    static std::false_type __test(...);

    static constexpr bool value = decltype(__test<T>(0))::value;
};
```

> 这里就给出这几个实现示例, 其实还可以有很多其他扩展形式, 关键的问题都在针对成员函数检测的部分提出过.


### 根据名称检测类型成员是否存在.

以检测类T中是否存在一个名为type的int类型数据成员为例.

---

实现这类检测的思路上比较简单, 就是尝试表达式`typename T::type`能否良好构造, 实现语法上也是可以灵活设计(不用局限于不求值操作数), 例如,

```cpp
template <typename T>
struct has_member_type_type
{
    template <typename U>
    static typename std::is_same<int, typename U::type>::type __test(int);

    template <typename>
    static std::false_type __test(...);

    static constexpr bool value = decltype(__test<T>(0))::value;
};
```

```cpp
template <typename T, typename = std::__void_t<>>
class has_member_type_type_helper
: public std::false_type {};

template <typename T>
class has_member_type_type_helper<T, std::__void_t<typename T::type>>
: public std::is_same<int, typename T::type>::type {};

template <typename T>
struct has_member_type_type
: public has_member_type_type_helper<T> {};
```

...

---
