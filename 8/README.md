http://llvm.org/docs/tutorial/LangImpl08.html

编译并输出到文件
不交叉编译的情况，只生成当前机器架构的机器码到文件
测试运行结果
```
localhost:build donghongchang$ ./llvmTest8 
ready> def func(x y) x+y;
ready> Read function definition:
define double @func(double %x, double %y) {
entry:
  %y2 = alloca double
  %x1 = alloca double
  store double %x, double* %x1
  store double %y, double* %y2
  %x3 = load double, double* %x1
  %y4 = load double, double* %y2
  %addtmp = fadd double %x3, %y4
  ret double %addtmp
}

ready> ready> ; ModuleID = 'My custom jit'
source_filename = "My custom jit"

define double @func(double %x, double %y) {
entry:
  %y2 = alloca double
  %x1 = alloca double
  store double %x, double* %x1
  store double %y, double* %y2
  %x3 = load double, double* %x1
  %y4 = load double, double* %y2
  %addtmp = fadd double %x3, %y4
  ret double %addtmp
}
```

将测试输出的 output.o 放到此目录下，使用
```
clang++ test.cpp output.o -o test
```
编译得到 test
运行 `./test` 可以看到从 c++ 调用 llvm 输出的 func 函数的结果
```
localhost:8 donghongchang$ clang++ test.cpp output.o -o test
localhost:8 donghongchang$ ./test 
func result of 3.0 and 4.0: 7
localhost:8 donghongchang$ 
```
