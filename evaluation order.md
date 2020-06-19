# C++11表达式求值顺序

### 求值(Evaluation)

C++11中, "求值(Evaluation)"的概念被作为术语定义于[1.9/12]:

*Evaluation of an expression (or a sub-expression) in general includes both value computations (including determining the identity of an object for glvalue evaluation and fetching a value previously assigned to an object for prvalue evaluation) and initiation of _side effects_*

同时这段定义中提到的"副作用(side effects)", 同样被作为术语被定义于[1.9/12]:

*Accessing an object designated by a volatile glvalue (3.10), modifying an object, calling a library I/O function, or calling a function that does any of those operations are all _side effects_, which are changes in the state of the execution environment.*

根据定义, 对表达式"求值"行为通常包括值计算和产生副作用这两个过程, 因此, "求值顺序"的含义也在此基础上被明确为表达式求值行为中的值计算与副作用产生的顺序.

### 优先级(precedence) & 结合性(associativity)

在C/C++中, 表达式(expression)是指由操作符和操作数构成的序列,它指定了一个运算. 所以当一个表达式由多个不同的操作符及操作数构成时(`a = b + c - d * e;`), 将面临一个操作符与操作数如何分组的问题, 如何分组将决定各个操作符到关联哪些操作数.
然而对于这种分组问题, C++标准中并没有以文本或列表的形式明确给出, 而是以每个操作符的表达式语法来表明<sup>[1]</sup>.
例如表达式`a = b + c - d * e;`中出现的`+`,`-`,`*`,`=`四个操作符, 其中`+`和`-`操作符对应的表达式的语法(简化)定义如下[5.7]:

    additive-expression:
        additive-expression + multiplicative-expression
        additive-expression - multiplicative-expression

    multiplicative-expression:
        pm-expression
        multiplicative-expression * pm-expression
        multiplicative-expression / pm-expression
        multiplicative-expression % pm-expression

     ...

从这个语法定义中可以看出, 当加法和减法表达式中存在乘法表达式(`multiplicative-expression`)时, 这个乘法表达式整体将被作为加/减表达式的操作数,
这意味着形如 `a+b*c` 这种表达式, `*`将先被分组变成(b\*c), 之后(b\*c)整体被作为`+`的右操作数, 所以原表达式变成 a + (b\*c).
像这种由语法定义表现出来的操作符与操作数分组次序就是操作符的优先级, 因此, 可以说`*`的优先级高于`+`, 同时因为`+`,`-`的表达式语法上定义上除了符号不同外,其余的都完全相同, 所以可以说`+`,`-`具有相同的优先级.

另外, 对于`additive-expression`的语法定义, 除了表明上面提到的优先级信息之外, 还可以看到`additive-expression`可以递归的由`additive-expression`构成, 也就是说,

    additive-expression + multiplicative-expression
    additive-expression - multiplicative-expression

可以展开成,

    additive-expression + multiplicative-expression + multiplicative-expression
    additive-expression - multiplicative-expression - multiplicative-expression

对比这两组语法定义可以看出, 当表达式中出现多个加法/减法表达式时, 左边的加/减表达式整体都将作为后一个加/减表达式的左操作数,
这意味着形如 `a + b + c` 这种表达式, 左边的 `+` 会先与操作数分组变成(a+b), 之后(a+b)整体被作为第二个`+`表达式的左操作数, 所以原表达式变成 (a+b) + c.

类似的, 对于`assignment-expression`, 也可以由其语法(简化)定义表明这种分组信息 [5.17/1 | A.4]:

    assignment-expression:
        logical-or-expression assignment-operator initializer-clause

    initializer-clause:
        assignment-expression

    ...

这个表达式的语法定义也是递归的构成, 所以它可以展开成:

    logical-or-expression assignment-operator logical-or-expression assignment-operator initializer-clause

对比这两个语法定义可以看出, 多个赋值操作符同时出现在一个表达式中时, 每个`assignment-operator`右边的表达式整体(以`initializer-clause`表示的部分)将被作为它的右操作数,
这意味着形如`a = b = c` 这种表达式中, 右边的`=`先与操作数分组, 变成(b=c), 之后整体被作为前一个`=`的右操作数, 所以原表达式变成 a = (b=c).

类似的, 像这种具有相同优先级的操作符与操作数的分组方式也就是操作符结合性<sup>[2]</sup>, 结合性只能从左到左右(例如`+`,`-`, ...)或从右到左(例如, `=`, `+=`, ...).
优先级与结合性共同构成了C++中的操作符与操作数分组的规则, 利用这些规则可以很容易的对表达式分组, 例如

    1 * 2 + 3 * 4
    
\* 优先级高于 + , 所以*先与操作数分组, 并且*是二元操作符,所以将与两个操作数分组, 因此(1 * 2) 将被分为组, (3 * 4)将被分为一组, 同样 + 也是二元操作符, 也许要与两个操作数分组, 所以(1 * 2)和(3 * 4)这两个组就被作为了+的操作数,所以原表达式分组后变成 (1 * 2) + (3 * 4).

    1 + 2 * 3 / 4
    
\+ 优先级低于 * 和 /, 所以 * 和 / 先与操作数分组, 同时 * 和 / 的优先级相同, 此时按照结合性分组, 而两个操作符的结合性都是 left-to-right, 并且两者都是二元操作符,需要两个操作数, 所以(2 * 3)将被划分为一组, ((2 * 3) / 4) 将被划分成一组, 同样+也需要两个操作数, 所以1和((2 * 3) /4)就分别作为左操作数和右操作数, 所以原表达式分组后变成 1 + ((2 * 3) / 4).

    *q++ = *p++
    
后缀 ++ 的优先级高于 *, 且两者都高于=, 所以 后缀++ 先与操作数分组, 根据 后缀++ 的结合性是left-to-right, 并且它属于一元操作符, 只与一个操作数结合, 所以两个++操作符分别分组成 (q++) 和 (p++), 之后一元 * 的结合性为 right-to-left, 也只与一个操作数分组, 所以两个*操作符被分别分组成 (*(q++))和(*(p++)), 最后分组=, 它需要两个操作数, 所以原表达式变成: (*(q++)) = (*(p++)).

    i++ + ++i * i

后缀++的优先级高于前缀++, 两者都高于*, 且三者都高于 +, 所以后缀++先分组, 根据其结合性为l-to-r, 且为一元操作符, 所以分组成 (i++), 而后, 前缀++进行分组,根据其结合性为right-to-left, 也是一元操作符, 所以分组成 (++i), 之后对二元*分组为 ((++i) * i), 最后对二元+分组, 原表达式变成 (i++) + ((++i) * i)

    -*p1 * *p2 * -*p3

这个表达式里面总共出现的运算符包括 - (负号), *(乘号), *(解引用), 其中-和解引用*的优先级相同,且都高于乘号*,并且解引用*和-的结合性都是right-to-left, 且都是一元操作符,只与一个操作数分组, 所以,三个一元*被分组成 (*p1), (*p2), (*p3), 两个-被分组成(-(*p1)), (-(*p3)), 最后两个乘号*的结合性为l-to-r, 且为二元运算符, 所以(-(*p1))与 (*p2)先被分组为((-(*p1)) * (*p2)), 再被分组为((-(*p1)) * (*p2)) * (*p3)

>[1] [Rationale for International Standard Programming Languages—C 6.5 30 Expressions (P59)](http://www.open-std.org/jtc1/sc22/wg14/www/docs/C99RationaleV5.10.pdf)
>
>[2] 在C中,结合性是由语法表明, 但是在C++中, 某些操作符的结合性是被明确说明的[5/*]

可以发现, 这些规则并不够直观和容易得出, 所以有很多的资料中都已经将它们整理成易读的形式格, 比如 [http://en.cppreference.com/w/cpp/language/operator_precedence]

### 优先级与结合性对求值顺序的影响.

从概念上来说两者并没有什么关系, 优先级与结合性是用来从语法上对构成表达式的操作符与操作数的分组规则, 并不是用来定义(子)表达式间的求值顺序的一套规则.
但是, 优先级的语法定义中可以看到, 被作为操作数的分组的结果的值计算总是要先于其所属的操作符表达式的结果的值计算,
也就是说 `a+b+c` 中, `(a+b)`的结果的计算总要先于`(a+b)+c`的结果的计算,  同样, 在`a+b\*c`中, `(b\*c)`的结果的计算总要先于`a+(b*c)`的结果的计算,
从这一方面来看, 优先级与结合性确实会间接的导致确定了一些值计算的顺序, 然而这一点在ANSI C/C99/C++98中关于求值顺序的地方是没有明确说明的, 所以最好不要依赖这一点,
但在C++11中, 这种值计算顺序被明确定义,

> *The value computations of the operands of an operator are sequenced before the value computation of the result of the operator.* [1.9/15]

### 关于求值顺序对求值结果的影响.

在前面引入了"求值"的定义, 知道一个求值通常包含值计算和引发副作用这两个过程,
如果对于一个仅包括值计算的表达式, 例如:
```cpp
i + i;
```
这种情况下, 其实对两个i的求值顺序并不会影响最终的求值结果, 因为在整个求值过程中i自始至终都存储着原本的值, 所以, 无论先求值哪个i,最终结果是一样的.
但是对于一个存在副作用的表达求值中, 求值顺序将变得很重要, 因为它可能影响着最终的求值结果, 例如:
```cpp
int i = 0;
int f(int& x) { return ++x; }
i + f(i);
```
针对`i+f(i)`这条表达式, 如果先求值f(i)再求值i得到的结果为2,
如果先求值i, 再求值f(i), 则得到的结果为1. 在这个例子中可以看到,
对同一个i的副作用与值计算的顺序不同, 导致的结果也不同, 但至少两种求值顺序都是符合C++11标准的, 具体结果视实现而定.
> 在clang++ 3.4.2 中, 结果为1, g++ 4.8.5中, 结果为2. (-std=c++11 -Wall)

然而在更坏的情况下, 求值顺序可能不止是导致求值结果不同, 甚至可能导致求值变成未定义行为, 例如:
```cpp
i + i++
```
在这个表达式中, 后缀++操作符的求值顺序是先进行值计算后引发副作用(+=1),对`i++`的值计算结果还是i的原始值,
所以"看起来"始终是两个i的值相加. 但实际上有个问题是, 在整个求值表达式中`i++`的副作用发生的时刻是不确定的, 也就是说, 如果先对i++求值, 则副作用发生于对`+`的左操作数i的求值之前和之后将是两种结果.

### 先序(sequenced before)

通过前面的例子可以看出, 对于带有副作用的表达式求值, 其求值结果将完全依赖于其求值顺序. 为了能够耕细粒度地描述两个求值间的顺序关系, C++11标准抛弃了基于"序列点(sequence points)"规则,
重新建立了以`先序(sequenced before)`关系为基础的规则, 并在先序(sequenced before)关系的基础上扩展了`无序(unsequenced)`和`不确定顺序(indeterminately sequenced)`两种关系[1.9/13]:

    Sequenced before is an asymmetric, transitive, pair-wise relation between evaluations executed by a single thread (1.10), which induces a partial order among those evaluations.

    Given any two evaluations A and B,

    + if A is sequenced before B, then the execution of A shall precede the execution of B.

    + If A is not sequenced before B and B is not sequenced before A, then A and B are unsequenced [Note: The execution of unsequenced evaluations can overlap. —end note ]
        [http://www.open-std.org/jtc1/sc22/wg21/docs/lwg-active.html#1459]

    + Evaluations A and B are indeterminately sequenced when either A is sequenced before B or B is sequenced before A, but it is unspecified which.
        [Note: Indeterminately sequenced evaluations cannot overlap, but either could be executed first. —end note ]

> Asymmetric relation 在这里是指给定两个求值A,B, 如果A sequenced before B, 则 B not sequenced before A.  
> Transitive relation 在这里是指给定三个求值A,B,C, 其中A sequenced before B并且B sequenced before C, 则 A sequenced before C.

这里提到的"无序(unsequenced)"和"不确定顺序(indeterminately sequenced)"的区别在于(待确认):

两个无序的求值之间可以交叉, 比如: ++i + ++j, 可能先算++i的副作用, 再算++j的副作用, 再对++i进行值计算, 再对++j进行值计算.   
两个不确定顺序的求值之间不能交叉, 必定有一个先执行, 比如: f(i++, other()), 可能先对i++求值, 再调用other()(调用之前必须完成i++的副作用), 或者先调用other()再对i++求值, 但是不能先对i++进行值计算, 再调用other(), 再完成i++的副作用.

### C++11表达式求值顺序定义

1) Except where noted, evaluations of operands of individual operators and of subexpressions of individual expressions are unsequenced. [C++11 1.9/15]

2) Every value computation and side effect associated with a full-expression is sequenced before every value computation and side effect associated with the next full-expression to be evaluated. [C++11 1.9/14]
每个完整表达式所关联的每个值计算和副作用都先先序于下一个完整表达式所关联的每个值计算和副作用.
```cpp
i++; i++; //第一个i++的值计算和副作用在第二个i++的值计算和副作用之前完成
```

3) The value computations of the operands of an operator are sequenced before the value computation of the result of the operator. [C++11 1.9/15]
这个定义意味着表达式`(a+b) * c`中的`a`和`b`的求值将先于对`a+b`的结果求值, 同时`a+b`的结果和`c`将先序于`(a+b) * c`的结果被求值. (但 a, b, 三者间的求值顺序根据1)是无序的)

4) When calling a function (whether or not the function is inline), every value computation and side effect
associated with any argument expression, or with the postfix expression designating the called function, is
sequenced before execution of every expression or statement in the body of the called function. [ Note: Value
computations and side effects associated with different argument expressions are unsequenced. — end note ] [1.9/15]

[Note: The evaluations of the postfix expression and of the argument expressions are all unsequenced relative to one another. All side effects of argument expression evaluations are sequenced before the function is entered (see 1.9). — end note ] [5.2.2/8]
这个定义表明所有函数实参的值计算和副作用都先序于函数体的执行, 但是各个实参之间的值计算和副作用都是无序的.
```cpp
void f(int& i) { i++; }
f(i++); //ok
f(i++, i++); //ub
```

5) Every evaluation in the calling function (including other function calls) that is not otherwise specifically
sequenced before or after the execution of the body of the called function is indeterminately sequenced with respect to the execution of the called function.
这个定义是说, 如果在一个函数调用表达式`A`中还包含着其他的函数调用表达式`B`, 那么`A`中其他的每个求值之间如果没有被指定先序或后序于`B`的函数体的执行, 则`A`中的其他求值与`B`的函数体执行是不确定顺序的.

```cpp
int i = 0;
void A(int i, int j) {

}
int B() {
    return i++;
}
A (i++, B()); //ok

int C(int a) {
    return a;
}

A(i++, C(i++)); //ub 这段定义只说明了i++相对于C的函数体是不确定顺序的, 而i++与C的实参i++之间的求值顺序还是无序的.
```

6) When a function is called, each parameter (8.3.5) shall be initialized (8.5, 12.8, 12.1) with its corresponding
argument. [ Note: Such initializations are indeterminately sequenced with respect to each other (1.9) — end note ] [5.2.2/4]

7) postfix ++/--
The value computation of the ++ expression is sequenced before the modification of the operand object.
With respect to an indeterminately-sequenced function call, **the operation of postfix++ is a single evaluation**.
[ Note: Therefore, a function call shall not intervene between the lvalue-to-rvalue conversion and the side effect associated with any single postfix ++ operator. — end note ]  [5.2.6]

8) prefix ++/-- 等价于 +=1/-=1, 所以其求值顺序等同于=/-=的求值顺序.

9) && and ||
If the second expression is evaluated, every value computation and side effect associated with the first expression is sequenced before every value computation and side effect associated with the second expression. [5.14/5.15]

10) ?:
Every value computation and side effect associated with the first expression is sequenced before every value computation
and side effect associated with the second or third expression. [5.16]

11) = *= /= %= += -= >>= <<= &= ˆ= |=
In all cases, the assignment is sequenced after the value computation of the right and left operands, and before the value computation of the assignment expression. With respect to an indeterminately-sequenced function call, the operation of a compound assignment is a single evaluation. [ Note: Therefore, a function call shall not intervene between the lvalue-to-rvalue
conversion and the side effect associated with any single compound assignment operator. — end note ] [5.17]

12) ,
Every value computation and side effect associated with the left expression is sequenced before every value computation and side effect associated with the right expression.


13) 除了2) ~ 12)外, 都按照1)的顺序求值.

14) If a side effect on a **scalar object** is unsequenced relative to either anotherside effect on the same scalar object or a value computation **using the value** of the same scalar object, the behavior is undefined. [1.9/15] 
这里明确定义无序(unsequenced)的求值顺序中的未定义行为.

### 表达式求值顺序分析

```cpp
int i = 0;
i + ++i;    //ub
i + i++;    //ub
```

对两个表达式求值都属于未定义行为. `+`操作符没有被定义其操作数的求值顺序, 所以左操作数i的值计算与++i/i++副作用是无序的(根据1), 而且左操作数i会发生一个左值到右值(lvalue-to-rvalue)的转换属于`using the value`, 所以符合14)

```cpp
int i = 0;
i = v[i++];  //ub
i = i++ + 1; //ub
```

对两个赋值表达式求值都属于未定义行为. 根据11), `=`左右操作数的值计算都先序于`=`的副作用, 但是没有说`=`左右操作数的副作用与`=`的副作用之间的顺序关系, 而根据1), 它们是无序的, 因此, `=`的副作用与`++`的副作用无序的发生于同一个i上, 符合14).

```cpp
int i = 0;
++i = i;    //ub
v[++i] = i; //ub
```

两个赋值表达式求值都是未定义行为. 根据8), `++i`的副作用先于其值计算, 根据1), `=`的左右操作数的求值是无序的, 所以左操作数`++i`的副作用与右操作数`i`的值计算是无序的, 所以属于未定义行为 14).

```cpp
int i = 0;
i = 7, i++, i++;    // i becomes 9
```

i的值变成9. 根据12), `,`表达式的每个操作数的值计算与副作用都先序于后一个操作数的值计算与副作用, 所以第一个i++求值完成后变成8, 第二个i++求值完成后变成9.

```cpp
int i = 0;
i = i + 1; // the value of i is incremented
```
i自增1; 根据11), `=`的左右操作数的值计算先于`=`的副作用, 而对左右操作数的值计算顺序虽然是无序的, 但是由于左右操作数的求值中不存在副作用, 因此整个求值是良好的.

```cpp
int i = 0;
void f(int, int);
f(i = -1, i = -1); // the behavior is undefined
```

对函数调用表达式求值属于未定义行为. 根据4), f的两个实参的求值顺序属于无序的, 而两个实参表达式`i = -1`对同一个标量对象i都带有副作用,  所以14).

```cpp
int i = 0;
i = ++i;    //same as i+=1;
```

赋值表达式等价于i+=1. 根据12), `++i`的副作用与`=`的左操作数的值计算是无序的, 但`=`不会像`+`一样导致左操作数`i`发生lvalue-to-rvalue的转换, 所以在这个赋值表达式中并没有使用左操作数i的值, 因此不符合14),
同时, 根据8), `++`的副作用先于`++i`的值计算, 根据11), 也先于`=`的副作用, 因此这个赋值表达式中对i的副作用是有序的， 也不符合14).

```cpp
int i = 0;

int f() {
    return i++;
}
void g(int x, int y) {
    printf("x = %d, y = %d\n", x, y);
}
g(i++, f());    // ok
```

x = 0, y = 1 或 x = 1, y = 0.
根据4), `i++`与`f()`的求值是无序的, 所以, 如果先求值`i++`再求值`f()`, 则实参为0,1, 如果先求值`f()`再求值`i++`, 则实参为1,0. 并且, 根据5), `i++`的求值相对于`f()`的函数体的执行是不确定顺序的, 所以两个`i++`的副作用的发生不符合14), 因此不是未定义行为.

```cpp
int f(int& i) {
    return i++;
}
void g(int x, int y) {
    printf("x = %d, y = %d\n", x, y);
}
int i = 0;
g(i++, f(i));    // ok
```

x = 0, y = 1 或 x = 1, y = 0.
根据4), `i++`与`f(i)`的求值是无序的, 所以, 如果先求值`i++`再求值`f(i)`, 则实参为0,1, 如果先求值`f(i)`再求值`i++`, 则实参为1,0. 并且, 根据5), `i++`的求值相对于`f(i)`的函数体的执行是不确定顺序的, 所以两个`i++`的副作用的发生不符合14),
并且, i是按引用传递给f, 所以这里?不会对i进行值计算or没有使用i的值? 这也不符合14), 因此不是未定义行为. [暂时不能确定按引用传参时是否对形参不进行值计算, 还是不使用形参的值]

```cpp
int f(int i) {
    return i++;
}
void g(int x, int y) {
    printf("x = %d, y = %d\n", x, y);
}
int i = 0;
g(i++, f(i));    // ub
```

对g的函数调用表达式求值属于未定义行为.
根据5), 标准只规定了 `i++`与f的函数体的执行是不确定顺序的, 但是没有任何标准能确定`i++`与f的实参i的求值顺序, 因此它们是无序的.
而f的实参i是按值传递, 所以需要发生左值到右值的转换, 算是使用i的值, 按照14), 这属于未定义行为.

```cpp
int x = 1, y = 2;
x += y += x += y;   //x += (y += (x += y))
```
对第一个x的值计算与第二个x的副作用是无序的, 属于未定义行为. 对y则没有这个问题, 因为第二个y的值计算一定先于对第一个y的副作用.


### 序列点(sequence points)

在C++98/03标准中, 使用序列点规则来确定表达式求值顺序[1.9/7]:

> At certain specified points in the execution sequence called sequence points, all side effects of previous evaluations
shall be complete and no side effects of subsequent evaluations shall have taken place.

通过对求值表达式中插入序列点来保证该序列点之前的求值(值计算)与副作用都已完成, 但这仅是确定了序列点前的求值先于序列点后, 而并没有确定一个序列点前或两个序列点间的求值(值计算)与副作用的先后顺序, 该顺序完全由实现决定.
例如表达式`(i++ > n) || m`, 在`(i++ > n)`之后会存在一个序列点, 所以, ++i的副作用和n的求值保证会在该序列点之前完成, 但是 ++i的副作用和n的求值之间的顺序却是由实现决定的.

C++98中, 序列点出现的位置有:

> 16 There is a sequence point at the completion of evaluation of each full-expression 10) .
>
> 17 When calling a function (whether or not the function is inline), there is a sequence point after the evaluation
of all function arguments (if any) which takes place before execution of any expressions or statements in
the function body. There is also a sequence point after the copying of a returned value and before the exe-
cution of any expressions outside the function 11) . Several contexts in C++ cause evaluation of a function
call, even though no corresponding function call syntax appears in the translation unit. [Example: evalua-
tion of a new expression invokes one or more allocation and constructor functions; see 5.3.4. For another
example, invocation of a conversion function (12.3.2) can arise in contexts in which no function call syntax
appears. ] The sequence points at function-entry and function-exit (as described above) are features of the
function calls as evaluated, whatever the syntax of the expression that calls the function might be.
>
> 18 In the evaluation of each of the expressions  
a && b  
a || b  
a ? b : c  
a , b  
using the built-in meaning of the operators in these expressions (5.14, 5.15, 5.16, 5.18), there is a sequence point after the evaluation of the first expression 12) .

除了序列点可以确定的求值顺序外, C++98/03中的表达式的求值顺序都是未指定行为[C++98 5/4]:
> Except where noted, the order of evaluation of operands of individual operators and subexpressions of individual expressions, and the order in which side effects take place, is unspecified.

明确定义序列点之间的求值中属于未定义行为的情况 [5/4]:
>Between the previous and next sequence point a scalar object shall have its stored value modified at most once by the evaluation of an expression. Furthermore, the prior value shall be accessed only to determine the value to be stored.
The requirements of this paragraph shall be met for each allowable ordering of the subexpressions of a full expression; otherwise the behavior is undefined.

### 其他参考:

>序列点到先序的过渡最初由[WG21/N1944]这份文档提出, 并在[WG21/n2171][WG21/N2239]等文档中重新修订.

>http://www.imooc.com/article/19755?block_id=tuijian_wz
https://www.eskimo.com/~scs/cclass/krnotes/sx5l.html
https://www.eskimo.com/~scs/readings/precvsooe.960725.html
https://stackoverflow.com/questions/5473107/operator-precedence-vs-order-of-evaluation
http://www.cs.man.ac.uk/~pjj/cs211/ho/node2.html
http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1008.htm
http://blog.csdn.net/pongba/article/details/541440
https://groups.google.com/forum/#!msg/comp.std.c++/3UskUQ7Rt48/UmyoY5sxLgIJ;context-place=topic/comp.std.c++/AXSqezSwalE
https://stackoverflow.com/questions/12321228/sequence-points-vs-operator-precedence?noredirect=1&lq=1
https://stackoverflow.com/questions/7506704/difference-between-sequence-points-and-operator-precedence-0-o
https://stackoverflow.com/questions/3852768/unsequenced-value-computations-a-k-a-sequence-points
http://www.doc88.com/p-8008701271311.html
https://stackoverflow.com/questions/19586893/double-assignment-of-the-same-variable-in-one-expression-in-c11
https://bytes.com/topic/c/answers/656382-expression-evaluation-order-precedence-rules
https://bytes.com/topic/c/answers/665454-c-c-ambiguity-order-evaluation
http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1188.pdf
https://bytes.com/topic/c/answers/665454-c-c-ambiguity-order-evaluation/2
https://bytes.com/topic/c/answers/682314-order-evaluation
https://stackoverflow.com/questions/4176328/undefined-behavior-and-sequence-points
https://blogs.msdn.microsoft.com/ericlippert/2008/05/23/precedence-vs-associativity-vs-order/
