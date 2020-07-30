# 模板实例化(Template Instantiation)

### 模板参数推导过程

文档迁移中...

<!--
14.8.2/6 两阶段替换意思:

Function template parameters are substituted (replaced by template arguments) twice:
explicitly specified template arguments are substituted before template argument deduction
deduced arguments and the arguments obtained from the defaults are substituted after template argument deduction

14.8.2/2  替换. 用显式指定的模板实参替换模板形参.
产生的函数模板并不完全(可能还带着需要被推导的模板参数).

14.8.2/3 调整. 调整上一步替换完成的函数模板的函数形参.

14.8.2/5 再替换. 对上一步调整完成的函数模板作为执行模板实参推导的类型,  如果没有被推导的模板实参给定默认值,
`当所有的(剩余, 或者说需要被推导的)模板实参都被推导完成或者取得默认值后, 再对模板实参的使用处替换`.


14.8.2/8 对无效的表达式或类型的定义:

If a substitution results in an invalid type or expression, type deduction fails. An invalid type or expression
is one that would be ill-formed if written using the substituted arguments. -->
