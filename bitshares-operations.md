
# BitShares 源码分析(1) - 操作(operations)

> 代码地址: https://github.com/bitshares/bitshares-core/tree/3.2.0


## BitShares 中的 "交易"

目前的主流公链中(Bitcoin, Ethereum, EOS), 链上数据状态的迁移是由一些被称为"交易"的行为来触发, 例如在EOS和Ethereum中, 交易实际上是对智能合约的方法调用, 智能合约的方法中有既定的逻辑对区块链的数据状态进行修改。这在Bitshares中也不例外, 它的链上数据状态迁移也需要有"交易"来触发。但是由于Bitshares不支持智能合约, 所以在"交易"的体现形式上不太一样。
Bitshares中的交易是由一个或多个预设的被称为`操作(operation)`的对象构成, 每个操作都有对应的业务处理逻辑。当一个交易被应用的时候, 这个交易中包含的每个操作会被依次执行相应的业务处理逻辑来修改区块链的状态。


Bitshares为每一种预设的操作都定义了一个相应的类来表示, 这些类的名字都以 `_operation` 结尾, 后续将称它们为"操作"或"操作类", 每个操作类都定义了与其业务相关的字段, 例如下面是 `transfer_operation` 操作类的定义

```cpp
///bitshares-core/libraries/protocol/include/graphene/protocol/base.hpp
struct base_operation
{
    //...
};

///bitshares-core/libraries/protocol/include/graphene/protocol/transfer.hpp
struct transfer_operation : public base_operation
{
    account_id_type  from;
    /// Account to transfer asset to
    account_id_type  to;
    /// The amount of asset to transfer from @ref from to @ref to
    asset            amount;

    /// User provided data encrypted to the memo key of the "to" account
    optional<memo_data> memo;

    //...
};
```

`transfer_operation` 操作类是一个转账交易, 所以字段中包含了几个信息是 `from` 转账人, `to` 转账目标, `amount` 转账的资产类型和数量, 以及 `memo` 附加信息. 类似的, 其他的操作也都具有一些各自的业务相关字段,这里就不一一列出。但是由于每个操作都具有不同的类型, 为了能对这些不同的类型实施统一的处理和传递, Bitshares专门定义了一个 `operation` 类型

```cpp
//bitshares-core/libraries/protocol/include/graphene/protocol/operations.hpp
typedef fc::static_variant<
    transfer_operation,
    limit_order_create_operation,
    limit_order_cancel_operation,
    call_order_update_operation,
    fill_order_operation,           // VIRTUAL
    account_create_operation,
    account_update_operation,
    account_whitelist_operation,
    account_upgrade_operation,
    account_transfer_operation,
    asset_create_operation,
    asset_update_operation,
    asset_update_bitasset_operation,
    asset_update_feed_producers_operation,
    asset_issue_operation,
    asset_reserve_operation,
    asset_fund_fee_pool_operation,
    asset_settle_operation,
    asset_global_settle_operation,
    asset_publish_feed_operation,
    ...
    > operation;
```

其中 `fc::static_variant` 是作者封装的一个类似于C++17的 `std::variant` 的类模板, 特点是在任意时刻只能存储一个在模板参数列表中列出的类型的值。而在这个定义中, 所有Bitshares支持的操作类都被作为了 `fc::static_variant` 的模板参数列表, 因此一个 `operation` 对象在运行时可以存储任何一个被支持的操作类对象, 所以只要能以某种规则从 `operation` 对象中识别出它是哪种操作的对象, 就可以与所有的这些操作类对象构成一种动态多态, 从而达到一致化的处理所有的操作类对象的目的.

## 求值器

在Bitshares的设计中, 操作与相应的业务处理代码是分离定义的, 也就是说, 每一个操作必然有一段相应的业务处理代码定义在其他的地方, 只有当一个操作的业务处理代码被执行, 才算真正的完成一个操作。 这些业务处理代码都实现在各个业务处理类中, 每一种操作都有一个对应的业务处理类, 这些类的名字都以 `_evaluator` 结尾, 后续称它们为"求值器"或"求值器类"。这些类所定义的源文件的文件名大都以 `_evaluator` 结尾, 例如:

+ bitshares-core/libraries/chain/account_evaluator.cpp
+ bitshares-core/libraries/chain/asset_evaluator.cpp
+ bitshares-core/libraries/chain/balance_evaluator.cpp
+ ...

下面就以 `account_create_evaluator` 的定义为例

```cpp
///bitshares-core/libraries/chain/account_evaluator.cpp
class account_create_evaluator : public evaluator<account_create_evaluator>
{
public:
   typedef account_create_operation operation_type;

   void_result do_evaluate( const account_create_operation& o );
   object_id_type do_apply( const account_create_operation& o ) ;
};

class account_update_evaluator : public evaluator<account_update_evaluator>
{
public:
   typedef account_update_operation operation_type;

   void_result do_evaluate( const account_update_operation& o );
   void_result do_apply( const account_update_operation& o );

   const account_object* acnt;
};

// ...
```

所有预设的操作类都有一个与之对应的求值器类, 例如, 账户创建的操作 `account_create_operation` 对应的求值器是`account_create_evaluator`. 每个求值器类中都有两个方法 `do_evaluate` 和 `do_apply`, 这两个方法就是这个类所对应的操作的业务逻辑, 也可以说是这个操作的"处理函数", 账户创建操作的求值器 `account_create_evaluator` 里边的这两个方法就是实现 `account_create_operation` 这个操作的业务逻辑, 即: 在Bitshares链上创建一个账户.

在这个求值器类的定义中, 每个求值器类还有一个基类是自身类型的 `evaluator` 实例, 例如 `account_create_evaluator` 继承自 `evaluator<account_create_evaluator>`, 其实这样做的目的是为了使所有的执行器都具有相同的调用入口, 从而可以以一致化的调用每个执行器。下面是 `evaluator` 类模板的定义, 接下来看它是如何做到这一点的.

```cpp
///bitshares-core/libraries/chain/include/graphene/chain/evaluator.hpp
class generic_evaluator
{
public:
    virtual ~generic_evaluator() {}
    virtual operation_result start_evaluate(transaction_evaluation_state &eval_state, const operation &op, bool apply);
    virtual operation_result evaluate(const operation &op) = 0;
    virtual operation_result apply(const operation &op) = 0;

    //...
};

template <typename DerivedEvaluator>
class evaluator : public generic_evaluator
{
public:
    virtual operation_result evaluate(const operation &o) final override
    {
        auto *eval = static_cast<DerivedEvaluator *>(this);
        const auto &op = o.get<typename DerivedEvaluator::operation_type>();
        //...
        return eval->do_evaluate(op);
    }

    virtual operation_result apply(const operation &o) final override
    {
        auto *eval = static_cast<DerivedEvaluator *>(this);
        const auto &op = o.get<typename DerivedEvaluator::operation_type>();
        //...
        auto result = eval->do_apply(op);

        //...
        return result;
    }
};

///bitshares-core/libraries/chain/evaluator.cpp
operation_result generic_evaluator::start_evaluate(transaction_evaluation_state &eval_state, const operation &op, bool apply)
{
    try
    {
        trx_state = &eval_state;
        //check_required_authorities(op);
        auto result = evaluate(op);

        if (apply)
            result = this->apply(op);
        return result;
    }
    FC_CAPTURE_AND_RETHROW()
}
```

通过对 `evaluator` 类模板的模板实例继承可以使每个执行器类继承到三个方法 `start_evaluate`, `evaluate`, `apply`, 其中 `start_evaluate` 方法又是来自于 `evaluator` 的基类 `generic_evaluator`, 通过这段代码可以看到一个求值器方法的调用链:

1. 首先求值器的 `do_evaluate` 和 `do_apply` 方法是由它们的基类 `evaluator` 的模板实例中的 `apply` 方法和 `execute` 方法来调用的。

2. 然后 `evaluator` 中的 `apply` 方法和 `execute` 又是由它的基类 `generic_evaluator` 中的 `start_evaluate` 方法来调用的。

所以这里得出一个结论是, 对每个求值器的真正调用入口是 `start_evaluate` 这个方法。这个设计是一种[模板方法模式(Template method pattern)](https://en.wikipedia.org/wiki/Template_method_pattern)的实现, 就是用它来对不同求值器实现一致化的调用。 

到这里明确一下操作和求值器的关系是: 操作定义了一个功能, 求值器实现了这个功能的逻辑。因此, 所有的操作以及相应的求值器共同定义了Bitshares的所有功能(上层业务逻辑)。

## 操作(operation)关联到相应的求值器(evaluator)

从实现方式上来说, Bitshares将每个操作和相应的求值器构建成键值对, 当在处理每个操作的时候, 就可以将操作作为"键"来找到相应的"值"(求值器). 这个构建过程起始于`database::initialize_evaluators`方法:

```cpp
///bitshares-core/libraries/chain/db_init.cpp
void database::initialize_evaluators()
{
   _operation_evaluators.resize(255);
   register_evaluator<account_create_evaluator>();
   register_evaluator<account_update_evaluator>();
   register_evaluator<account_upgrade_evaluator>();
   register_evaluator<account_whitelist_evaluator>();
   ...
}

//bitshares-core/libraries/chain/include/graphene/chain/database.hpp
template<typename EvaluatorType>
void register_evaluator()
{
    _operation_evaluators[
    operation::tag<typename EvaluatorType::operation_type>::value].reset( new op_evaluator_impl<EvaluatorType>() );
}

//bitshares-core/libraries/chain/include/graphene/chain/database.hpp
vector< unique_ptr<op_evaluator> > _operation_evaluators;
```

可以看到这个键值对的本质就是一个数组. 所以它的键就是数组的索引, 它的值就是这个数组的元素类型 `unique_ptr<op_evaluator>`. 构建过程首先是在 `initialize_evaluators` 方法中为所有的求值器类型都调用了 `register_evaluator`, 每一次调用都会产生一条键值对, 调用的同时通过模板参数将被注册的求值器类型传递过去。 另一边 `register_evaluator` 的模板形参 `EvaluatorType` 就是那些被注册的求值器类型, 比如 `account_create_evaluator`, `account_update_evaluator` ...

接下来拆分这个键值对的构成, 首先来看这个键值对中的键, 也就是下标:

```cpp
operation::tag<typename EvaluatorType::operation_type>::value
```

其中 `EvaluatorType` 就是每次调用 `register_evaluator` 时传递的求值器类,
所以 `typename EvaluatorType::operation_type` 的意思就是引用求值器类型中的一个名为`operation_type`的类型成员. 回归上面`account_create_evaluator`的代码可以发现, 每个求值器类确实都有一个`operation_type`, 这个类型就是这个求值器类所关联到的操作(operation)。

在下面的例子中 `typename S::value_type` 就类似于 `typename EvaluatorType::operation_type`.

```cpp
#include <type_traits>

struct S {
    typedef int value_type;
};

int main() 
{
    typename S::value_type int_value = 123;
    std::is_same<
        typename S::value_type, 
        int
    >::value == 1; //true;
    return 0;
}
```

再来看 `operation::tag<T>::value` 这一部分, 根据前面引用的代码中对 `operation` 类型定义:

```cpp
typedef fc::static_variant<transfer_operation, ...> operation;
```

换种写法就是

```cpp
fc::static_variant<transfer_operation, ...>::template tag<T>::value;
```

> 注: 上面两段代码中的 "..." 是省略号, 表示所有的操作类(operations), 不是模板参数包.


这个表达式的意思是计算"类型`T`位于`static_variant`的模板参数列表中的位置", 例如 `fc::static_variant<A, B, C, D, E>::template tag<C>::value` 的结果为 `2`

那么`operation::tag<typename EvaluatorType::operation_type>::value` 的意思就是: 先把求值器类中关联的操作类型`operation_type` 找出来, 然后再从`static_variant` 中把该操作类型位于所有的操作类型的索引找出来, 就以这个索引作为键值对的key.

下面的代码演示查找索引

```cpp
//g++ test.cpp  -std=c++11

#include <type_traits>
#include <iostream>

struct account_create_operation
{
};
struct account_update_operation
{
};
struct account_whitelist_operation
{
};

struct account_create_evaluator
{
    typedef account_create_operation operation_type;
};
struct account_update_evaluator
{
    typedef account_update_operation operation_type;
};
struct account_whitelist_evaluator
{
    typedef account_whitelist_operation operation_type;
};

template <typename... Tn>
struct static_variant_demo
{
    //这个模板就是从一堆类型`Args`中查找类型`Tp`, 依靠类模板特化和递归实例化.
    template <typename Tp, typename... Args>
    struct __index_of_helper;

    //当实例化匹配到这个特化版本时, 意味着模板参数列表中剩下的最后一个类型时仍然没有找到匹配的类型则返回-1
    template <typename Tp>
    struct __index_of_helper<Tp> : public std::integral_constant<int, -1>
    {
    };

    //当实例化匹配到这个特化版本意味着找到了匹配的类型.
    template <typename Tp, typename... Args>
    struct __index_of_helper<Tp, Tp, Args...>
        : public std::integral_constant<int, 0>
    {
    };

    //实例化这个版本意味着没有找到匹配的类型, 则继续递归实例化.
    template <typename Tp, typename Up, typename... Args>
    struct __index_of_helper<Tp, Up, Args...>
        : public std::integral_constant<int, __index_of_helper<Tp, Args...>::value == -1 ? -1 : __index_of_helper<Tp, Args...>::value + 1>
    {
    };

    template <typename Tp>
    using __index_of = __index_of_helper<Tp, Tn...>;

    template <typename X, typename = typename std::enable_if<__index_of<X>::value != -1, X>::type>
    struct tag
    {
        static const int value = __index_of<X>::value;
    };
};

typedef static_variant_demo<
    account_create_operation,
    account_update_operation,
    account_whitelist_operation
> operation;

template <typename EvaluatorType>
void register_evaluator()
{
    std::cout << operation::tag<typename EvaluatorType::operation_type>::value << std::endl;
}

void initialize_evaluators() {
    register_evaluator<account_create_evaluator>();
    register_evaluator<account_update_evaluator>();
    register_evaluator<account_whitelist_evaluator>();
}

int
main()
{
    initialize_evaluators();    //0, 1, 2

    return 0;
}
```

接下来跟踪这个键值对中的值的构成, 也就是 `new op_evaluator_impl<EvaluatorType>()` 这个表达式

```cpp
///bitshares-core/libraries/chain/include/graphene/chain/evaluator.hpp
class op_evaluator
{
public:
    virtual ~op_evaluator() {}
    virtual operation_result evaluate(
        transaction_evaluation_state &eval_state, const operation &op, bool apply) = 0;
};

template <typename T>
class op_evaluator_impl : public op_evaluator
{
public:
    virtual operation_result evaluate(
        transaction_evaluation_state &eval_state, const operation &op, bool apply = true) override
    {
        T eval;
        return eval.start_evaluate(eval_state, op, apply);
    }
};
```

从代码中可知, `op_evaluator_impl::evaluate` 构造了一个执行器对象`eval`(T就是执行器的类型), 并调用它的 `start_evaluate`方法. 前面说过, `start_evaluate` 方法又会调用 `evaluate` 方法, `evaluate`方法又会调用`do_evaluate`方法。 所以`new op_evaluator_impl<EvaluatorType>()`这个表达式就是构造了一个可以调用`EvaluatorType::start_evaluate`方法的对象。 由于`op_evaluator_impl`的每个实例都是不同的类型, 比如`op_evaluator_impl<account_create_evaluator>` 和 `op_evaluator_impl<account_update_evaluator>` 就是完全不同的类型, 所以这里借助多态性, 使用基类型 `op_evaluator` 作为键值对中的值.


到这里操作与其求值器的关联就建立完成了。 接下来看一下如何查找和调用求值器的。

```cpp
//bitshares-core/libraries/chain/db_block.cpp
operation_result database::apply_operation(transaction_evaluation_state &eval_state, const operation &op)
{
    try
    {
        int i_which = op.which();
        uint64_t u_which = uint64_t(i_which);
        FC_ASSERT(i_which >= 0, "Negative operation tag in operation ${op}", ("op", op));
        FC_ASSERT(u_which < _operation_evaluators.size(), "No registered evaluator for operation ${op}", ("op", op));
        unique_ptr<op_evaluator> &eval = _operation_evaluators[u_which];
        FC_ASSERT(eval, "No registered evaluator for operation ${op}", ("op", op));
        auto op_id = push_applied_operation(op);
        auto result = eval->evaluate(eval_state, op, true);
        set_applied_operation_result(op_id, result);
        return result;
    }
    FC_CAPTURE_AND_RETHROW((op))
}
```

第一步就是先通过 `op.which()` 方法找到操作`op`的键, 也就是在构建键值对的时候的索引.
第二步就是拿这个索引去当时注册的表`_operation_evaluators`里查找相应的执行器 `eval`
第三步就是调用执行器的`evaluate`方法


### 未完待续
<!-- 
```cpp
#include <variant>
#include <iostream>
#include <string>

struct address
{
private:
    std::string _address;
public:
};
struct uint256
{
private:
    std::uint64_t _value[4];
public:
};

struct WETH
{
    
    bool transfer(address from, address to, uint256 amount)
    {
        std::cout << "WETH::transfer" << std::endl;
        return true;
    }

    bool approve(address spender, uint256 amount)
    {
        std::cout << "WETH::approve" << std::endl;
        return true;
    }
};

struct USDT
{
    bool transfer(address from, address to, uint256 amount)
    {
        std::cout << "USDT::transfer" << std::endl;
        return true;
    }

    bool approve(address spender, uint256 amount)
    {
        std::cout << "USDT::approve" << std::endl;
        return true;
    }
};

typedef std::variant<WETH, USDT> token_t;

int
main()
{
    address from, to, spender;
    uint256 amount;

    token_t token1(WETH{}); 
    std::get<WETH>(token1).transfer(from, to, amount);
    std::get<WETH>(token1).approve(spender, amount);

    token_t token2(USDT{}); 
    std::get<USDT>(token2).transfer(from, to, amount);
    std::get<USDT>(token2).approve(spender, amount);
}
``` -->