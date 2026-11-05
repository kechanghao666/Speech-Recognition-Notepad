# 语音识别记事本软件

Windows原生桌面程序，使用C++17和系统组件实现。

## 功能

- 选择本机语音识别引擎
- 通过麦克风输入连续听写
- 导入PCM WAV文件识别
- 切换听写与限定命令模式
- 显示识别文本与原始日志
- 编辑并保存UTF-8笔记

## 编译与运行

准备Windows x64和MinGW-w64 C++工具链，在项目目录执行：

```powershell
.\build.ps1 -Compiler "C:\mingw64\bin\g++.exe"
.\SpeechNotes.exe
```

也可将g++加入PATH后运行 `.\build.ps1`。工程另提供CMake配置。

## 操作

选择与输入语言匹配的桌面SAPI识别引擎，使用默认麦克风或“识别 WAV”导入音频。听写结果可以编辑并保存为UTF-8笔记；命令模式支持换行、标点、保存和停止。examples中的音频由语音合成生成，用于文件输入演示。

## 文件

- src：窗口、处理算法及文件输入输出。
- build.ps1：Windows编译入口。
- CMakeLists.txt：CMake构建配置。

作者：柯长浩。仓库保留课程实验窗口标题。
