http://llvm.org/docs/tutorial/LangImpl09.html

记录并使用调试信息
测试编译运行
```
/Users/donghongchang/Desktop/Projects/llvmTest/cmake-build-debug/9/llvmTest9
ready> def func(x y) x+y
ready> ;
Read function definition:
define double @func(double %x, double %y) !dbg !2 {
entry:
  %y2 = alloca double
  %x1 = alloca double
  store double %x, double* %x1
  store double %y, double* %y2
  %x3 = load double, double* %x1, !dbg !7
  %y4 = load double, double* %y2, !dbg !7
  %addtmp = fadd double %x3, %y4, !dbg !7
  ret double %addtmp, !dbg !7
}

ready> ~
ready> ; ModuleID = 'My custom jit'
source_filename = "My custom jit"

define double @func(double %x, double %y) !dbg !2 {
entry:
  %y2 = alloca double
  %x1 = alloca double
  store double %x, double* %x1
  store double %y, double* %y2
  %x3 = load double, double* %x1, !dbg !7
  %y4 = load double, double* %y2, !dbg !7
  %addtmp = fadd double %x3, %y4, !dbg !7
  ret double %addtmp, !dbg !7
}

!llvm.dbg.cu = !{!0}

!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "Kaleidoscope Compiler", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!1 = !DIFile(filename: "fib.ks", directory: ".")
!2 = distinct !DISubprogram(name: "func", scope: !1, file: !1, type: !3, isLocal: false, isDefinition: true, flags: DIFlagPrototyped, isOptimized: false, unit: !0, variables: !6)
!3 = !DISubroutineType(types: !4)
!4 = !{!5, !5, !5}
!5 = !DIBasicType(name: "double", size: 64, align: 64, encoding: DW_ATE_float)
!6 = <temporary!> !{}
!7 = !DILocation(line: 0, scope: !2)
```
