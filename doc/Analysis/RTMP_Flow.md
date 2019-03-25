# Flow

[TOC]



## 一：Handshake

### 1.1 简单握手

```c++
CLIENT										SERVER

C0					--------------->
					<---------------		S0

C1					--------------->
					<---------------		S2

					<---------------		S1
C2					--------------->					
```

简述流程：

客户端发送C0发起握手，服务端回复S0表示同意握手；之后客户端和服务端分别发送握手验证消息C1和S1；服务端收到客户端的C1回复S2给客户端，此时客户端握手完成；客户端收到服务端的S1回复C2给服务端，此时服务端握手完成。为了提高效率，客户端往往会同时发送C0和C1，服务端在接收到C0后也会同时发送S0和S1。

### 1.2 复杂握手

略





## 二：推流

```c++
CLIENT										SERVER

connect				--------------->	  
					<---------------		set server BW
					<---------------		set client BW
					<---------------		set chunk size
					<---------------		_result for NetConnection.Connect.Success
			
releaseStream		--------------->
FCPublish			--------------->
createStream		--------------->
					<---------------		_result for createStream

publish				--------------->
					<---------------		(onStatus)NetStream.Publish.Start
					
onMetaData			--------------->
video data			--------------->
audio data			--------------->
```





## 三：拉流

略







参考：

[rtmp信令交互过程分析一-概述](https://blog.csdn.net/Jacob_job/article/details/81866127)