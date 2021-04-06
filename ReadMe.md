1. 进入test目录下创建input.txt 为传输的源文件
2. 进入src目录下执行如下命令生成可执行程序
```
   gcc receiver.c SimpleTcp.c -o receiver -I../include

   gcc sender.c SimpleTcp.c -o sender -lpthread -I../include
```
  
3. 测试GBN模式 分别执行如下命令
    ```
      ./receiver -p GBN -l 100 -s 1 -r 100000
      ./sender -p GBN -l 100 -s 10 -r 100000 -t 5
    ```
>- -p 表示protocol类型为GBN类型 -l 表示每次传输数据的大小 -s 滑动窗口的大小 -r(最大范围的seq 暂时没有用到 不清楚超过范围后是循环还是停止) -t 超时的时间

4. 测试SR模式
   ```
        ./receiver -p SR -l 100 -s 10 -r 100000 
        ./sender -p SR -l 100 -s 10 -r 100000 -t 5
   ```
>参数代表含义如上

**目前在程序里固定为seq = 10 和 ack = 10时会丢包 如果需要随机生成或用户指定再修改**

1. 程序运行结束后 进入test目录下会生成output.txt的输出文件 比较输入与输出文件
    ```
        md5 input.txt
        md5 output.txt
    ```