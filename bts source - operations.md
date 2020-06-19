
# BTS 源码分析(1) - 操作的解析

### 前言

本系列文章无意于详细解释BTS的业务逻辑, 只会描述存储模型, 出块算法, 交易和块的打包验证等底层架构。 内容要求了解C++相关知识, 内容要求了解BTS项目.

> 代码地址: https://github.com/bitshares/bitshares-core/tree/3.2.0


### BTS 中的 "交易"

目前的主流公链中(Bitcoin, Ethereum, EOS), 链上数据状态的迁移是由一些被称为"交易"的行为来触发, 例如在EOS和Ethereum中, 交易本质上是对智能合约的方法调用, 智能合约的方法中有既定的逻辑对区块链的数据状态进行修改。所以在BTS中也不例外, 它的链上数据状态迁移也需要有"交易"来触发。但是由于BTS不支持智能合约, 所以在"交易"的体现形式上不太一样。
BTS中的交易是由一些预设的被称为操作(operations)的枚举来表示, 对于每个指令都有着对应的处理逻辑。基于此可以将BTS理解为底层是一条基于DPOS共识的链, 上层是一个不可变的智能合约.

BTS中的这些操作(operations)被定义在 `bts/bitshares-core/libraries/protocol/include/graphene/protocol/operations.hpp`

```cpp
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

这里的 `fc::static_variant` 是BM封装的一个类似于C++17的`std::variant`的东西, 作用是在任意时刻只能存储一个模板参数列表中列出的类型的值. 

### 对"操作"的处理

前面说过, 所有的操作都是预设的, 也就是说这条公链的除了底层的共识和存储部分之外, 其余的对每种操作的处理逻辑也都包含在这条公链中。相应的处理代码文件名大都以 `_executor` 结尾, 例如:

+ bts/bitshares-core/libraries/chain/account_evaluator.cpp
+ bts/bitshares-core/libraries/chain/asset_evaluator.cpp
+ bts/bitshares-core/libraries/chain/balance_evaluator.cpp
+ ...

接下来就以 `account_evaluator.cpp` 的代码为例。

```cpp
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

...
```

文件中定义了许多类(class), 每个类对应于每一种操作的处理。比如账户创建的操作 `account_create_operation`, 就有一个对应的类 `account_create_evaluator`。 同时每个类中都有两个方法:

```cpp
void_result do_evaluate( const account_whitelist_operation& o);

void_result do_apply( const account_whitelist_operation& o);
```

每个类这两个方法就根据要处理的操作不同实现不同的业务逻辑。

可以看到每个`evaluator`类都继承各自类型的`evaluator`实例, 也就是`account_update_evaluator`继承自`evaluator<account_create_evaluator>`, 
`account_update_evaluator` 继承自`evaluator<account_update_evaluator>`。

一段简单的代码来模拟BTS中的这段设计

```cpp
#include <variant>

struct A
{
};

struct B
{
};

typedef std::variant<A, B> operation;

class generic_evaluator
{
public:
    virtual ~generic_evaluator() {}
    virtual void evaluate(const operation &op) = 0;
    virtual void apply(const operation &op) = 0;
};

template <typename DerivedEvaluator>
class evaluator : public generic_evaluator
{
public:
    virtual void evaluate(const operation &o) final override
    {
        auto *eval = static_cast<DerivedEvaluator *>(this);
        const auto &op = std::get<typename DerivedEvaluator::operation_type>(o);
        return eval->do_evaluate(op);
    }

    virtual void apply(const operation &o) final override
    {
        auto *eval = static_cast<DerivedEvaluator *>(this);
        const auto &op = std::get<typename DerivedEvaluator::operation_type>(o);
        return eval->do_apply(op);
    }

    virtual ~evaluator() {}
};

class A_evaluator : public evaluator<A_evaluator>
{
public:
    typedef A operation_type;
    void do_evaluate(const A &o);
    void do_apply(const A &o);
    virtual ~A_evaluator() {}
};

int main()
{
    return 0;
}
```

通过这个简化版的模型很容易发现, 这就是一种模板方法模式的实现。到这里我们知道了BTS是如何一致化的处理各种不同的操作(operations), 接下来跟踪一下具体如何执行一个操作。