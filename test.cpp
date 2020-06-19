
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