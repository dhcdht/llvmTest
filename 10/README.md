http://llvm.org/docs/tutorial/LangImpl10.html

整理代码、添加注释准备 KaleidoscopeJIT 的实现

回归测试用的代码
```
def foo(x y) x+foo(y, 4.0);
extern sin(a);
4+5;
def func(a b) a*a + 2*a*b + b*b;
def bar(a) func(a, 4.0) + bar(31337);
----
extern fab();
extern foo();
def f(x) if x then foo() else fab();
----
extern f(c);
def pr(n) for i = 1, i < n, 1.0 in f(1);
----
def binary : 1 (x y) 0;
extern print(x);
print(123) : print(456) : print(789);
----
def binary : 1 (x y) y;def fibi(x) var a = 1, b = 1, c in (for i = 3, i < x in c = a+b:a=b:b=c):b;
```
测试运行成功
