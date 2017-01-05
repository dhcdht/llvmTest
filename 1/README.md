http://llvm.org/docs/tutorial/LangImpl02.html

实现能够解析下边 Kaleidoscope 代码的编译器
```
$ ./a.out
ready> def foo(x y) x+foo(y, 4.0);
Parsed a function definition.
ready> def foo(x y) x+y y;
Parsed a function definition.
Parsed a top-level expr
ready> def foo(x y) x+y );
Parsed a function definition.
Error: unknown token when expecting an expression
ready> extern sin(a);
ready> Parsed an extern
ready> ^D
$
```


当前代码使用
```
clang++ -std=c++11 toy.cpp
```
编译通过

测试运行结果
```
localhost:1 donghongchang$ clang++ -std=c++11 toy.cpp 
localhost:1 donghongchang$ ./a.out
ready> def foo(x y) x+foo(y, 4.0);
ready> Parsed a function definition.
ready> def foo(x y) x+y y;
ready> Parsed a function definition.
ready> Parsed a top-level expr.
ready> def foo(x y) x+y );
ready> Parsed a function definition.
ready> LogError: Unknown token when expecting an expression
ready> extern sin(a);
ready> Parsed an extern.
ready> ^C
localhost:1 donghongchang$
```
