http://llvm.org/docs/tutorial/LangImpl04.html

toy.cpp 代码基于上一章的 toy.cpp

当前的 llvm 版本是会自动优化编译时期代码的，所以此章的代码不必须添加
测试代码
```
localhost:4 donghongchang$ clang++ -std=c++11 `llvm-config --cxxflags --ldflags --system-libs --libs core` toy.cpp
localhost:4 donghongchang$ ./a.out 
ready> def test(x) 1+2+x;
ready> Read function definition:
define double @test(double %x) {
entry:
  %addtmp = fadd double 3.000000e+00, %x
  ret double %addtmp
}

ready> 
```
可以看到已经是优化过的，而不是原始的 
```
ready> def test(x) 1+2+x;
Read function definition:
define double @test(double %x) {
entry:
        %addtmp = fadd double 2.000000e+00, 1.000000e+00
        %addtmp1 = fadd double %addtmp, %x
        ret double %addtmp1
}
```

顺便重构优化一下结构
