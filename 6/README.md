http://llvm.org/docs/tutorial/LangImpl06.html

实现用户可自定义运算符

实现自定义二元运算符
测试运行
```
// 定义 ":" 为优先级为 1 的二元运算符，并且返回 0
ready> def binary : 1 (x y) 0;
ready> Read function definition:
define double @"binary:"(double %x, double %y) {
entry:
  ret double 0.000000e+00
}

// 使用刚刚自定义的 ":" 运算符
ready> printd(123) : printd(456) : printd(789);
ready> Read top-level expr:
define double @0() {
entry:
  %calltmp = call double @printd(double 1.230000e+02)
  %calltmp1 = call double @printd(double 4.560000e+02)
  %binop = call double @"binary:"(double %calltmp, double %calltmp1)
  %calltmp2 = call double @printd(double 7.890000e+02)
  %binop3 = call double @"binary:"(double %binop, double %calltmp2)
  ret double %binop3
}

ready>
```


实现自定义一元运算符
测试运行
```
// 自定义 "!" 运算符
define double @"unary!"(double %x) {
entry:
  %ifcondition = fcmp one double %x, 0.000000e+00
  br i1 %ifcondition, label %then, label %else

then:                                             ; preds = %entry
  br label %ifcont

else:                                             ; preds = %entry
  br label %ifcont

ifcont:                                           ; preds = %else, %then
  %iftmp = phi double [ 0.000000e+00, %then ], [ 1.000000e+00, %else ]
  ret double %iftmp
}

ready> 
```
