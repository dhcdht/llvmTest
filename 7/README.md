http://llvm.org/docs/tutorial/LangImpl07.html

这一章介绍了一下 SSA (变量单赋值)，并且 llvm 只有 SSA 模式，在这种条件下我们应该怎么给变量赋值
前边代码中使用的 phi node 是 llvm 提供的一种方式，
通过使用函数栈来读写变量也是一种方式，
函数栈方式并不高效，llvm 有内置的 "mem2reg" 会将其优化为使用 phi node 的 IR 代码，

使用函数栈来读写变量
测试运行
```
localhost:build donghongchang$ ./llvmTest7 
ready> def binary : 1 (x y) y;def fibi(x) var a = 1, b = 1, c in (for i = 3, i < x in c = a+b:a=b:b=c):b;
ready> Read function definition:
define double @"binary:"(double %x, double %y) {
entry:
  %y2 = alloca double
  %x1 = alloca double
  store double %x, double* %x1
  store double %y, double* %y2
  %y3 = load double, double* %y2
  ret double %y3
}

ready> ready> Read function definition:
define double @fibi(double %x) {
entry:
  %i = alloca double
  %c = alloca double
  %b = alloca double
  %a = alloca double
  %x1 = alloca double
  store double %x, double* %x1
  store double 1.000000e+00, double* %a
  store double 1.000000e+00, double* %b
  store double 0.000000e+00, double* %c
  store double 3.000000e+00, double* %i
  br label %loop

loop:                                             ; preds = %loop, %entry
  %c2 = load double, double* %c
  %a3 = load double, double* %a
  %b4 = load double, double* %b
  %addtmp = fadd double %a3, %b4
  store double %addtmp, double* %c
  %a5 = load double, double* %a
  %b6 = load double, double* %b
  store double %b6, double* %a
  %binop = call double @"binary:"(double %addtmp, double %b6)
  %b7 = load double, double* %b
  %c8 = load double, double* %c
  store double %c8, double* %b
  %binop9 = call double @"binary:"(double %binop, double %c8)
  %i10 = load double, double* %i
  %x11 = load double, double* %x1
  %cmptmp = fcmp ult double %i10, %x11
  %booltmp = uitofp i1 %cmptmp to double
  %0 = load double, double* %i
  %nextVar = fadd double %0, 1.000000e+00
  store double %nextVar, double* %i
  %loopcond = fcmp one double %booltmp, 1.000000e+00
  br i1 %loopcond, label %loop, label %afterloop

afterloop:                                        ; preds = %loop
  %b12 = load double, double* %b
  %binop13 = call double @"binary:"(double 0.000000e+00, double %b12)
  ret double %binop13
}

ready> 
```
