# android-ninja-to-compdb
## 功能说明
解析android模块编译产生ninja文件，生成compile_commands.json文件.

## 编译
```shell
mkdir build && cd build && cmake .. && make
```

## 运行
1. 将编译生成的an-compdb加入$PATH的路径中
2. 进入android工程根目录
3. an-compdb out/xxxx.ninja,在当前目录下生成compile_commands.json文件.
