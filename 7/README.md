http://llvm.org/docs/tutorial/LangImpl07.html

这一章介绍了一下 SSA (变量单赋值)，并且 llvm 只有 SSA 模式，在这种条件下我们应该怎么给变量赋值
前边代码中使用的 phi node 是 llvm 提供的一种方式，
通过使用函数栈来读写变量也是一种方式，
函数栈方式并不高效，llvm 有内置的 "mem2reg" 会将其优化为使用 phi node 的 IR 代码，
