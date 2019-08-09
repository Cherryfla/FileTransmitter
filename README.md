# FileTransmitter

## 使用方法
make之后
server: ./server
client: ./client ip

## 在client中
+ 使用 GET dir 来获取目录下的文件列表
+ 使用 GET file 来得到文件
+ 使用 INFO 来获取一天内各命令执行次数（需要redis支持）
+ 使用 QUIT 退出

## 值得改进的地方
+ 只支持绝对路径
+ 文件传输连接建立需要等待,否则可能导致无法建立连接
+ 可移植性
+ 传输文件性能
