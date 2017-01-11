http://llvm.org/docs/tutorial/LangImpl05.html

toy.cpp 代码基于上一章的 toy.cpp

实现控制流语句
测试运行
```
localhost:build donghongchang$ ./llvmTest5 
ready> extern foo();
ready> Read extern:
declare double @foo()

ready> extern fab();
ready> Read extern:
declare double @fab()

ready> def f(x) if x then foo() else fab();
ready> Read function definition:
define double @f(double %x) {
entry:
  %ifcondition = fcmp one double %x, 0.000000e+00
  br i1 %ifcondition, label %then, label %else

then:                                             ; preds = %entry
  %calltmp = call double @foo()
  br label %ifcont

else:                                             ; preds = %entry
  %calltmp1 = call double @fab()
  br label %ifcont

ifcont:                                           ; preds = %else, %then
  %iftmp = phi double [ %calltmp, %then ], [ %calltmp1, %else ]
  ret double %iftmp
}

ready> 
localhost:build donghongchang$ 
```


