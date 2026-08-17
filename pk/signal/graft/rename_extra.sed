# 试接脚手架 —— 不是交付物。老式 SIGNAL()/SLOT() 宏的正式全仓转换归 S 批次。
# 这里写死 sender/receiver 都是 TestClass（KisSignalAutoConnectionTest 的事实），
# 全仓别处 sender 类型千变万化，S 批次不能复用本文件当通用规则。
#
# 注意：sed 替换串里 `&` 是「整个匹配文本」的特义字符，凡是想输出字面 `&`
# （成员函数指针的 `&TestClass::xxx`、QOverload 模板参数里的 `const QString &`）
# 都必须写成 `\&`。brief 里的裸 `&` 会产出 `SIGNAL(sigTest1())TestClass::sigTest1`
# 这种坏文本，这里已逐条转义。
s/\bSIGNAL(sigTest1())/\&TestClass::sigTest1/g
s/\bSLOT(slotTest1())/\&TestClass::slotTest1/g
s/\bSIGNAL(sigTest2(int))/QOverload<int>::of(\&TestClass::sigTest2)/g
s/\bSLOT(slotTest2(int))/QOverload<int>::of(\&TestClass::slotTest2)/g
s/\bSIGNAL(sigTest2(const QString &, const QString &))/QOverload<const QString \&, const QString \&>::of(\&TestClass::sigTest2)/g
s/\bSLOT(slotTest2(const QString &))/QOverload<const QString \&>::of(\&TestClass::slotTest2)/g