# TypeStatus

![TypeStatus 图标](resources/app-icon.svg)

TypeStatus 是一个轻量的 Windows 输入状态提示工具。它会根据当前输入法的中英文模式改变鼠标光标颜色，让你不用查看任务栏，也能知道下一次输入的是中文还是英文。

| 光标颜色 | 当前状态 |
| --- | --- |
| 红色 | 中文输入 |
| 蓝色 | 英文输入 |
| Windows 原本的颜色 | 暂停中，或当前状态无法可靠判断 |

> 当前版本为 **v0.1.1 MVP**，适合试用和反馈。

## 下载

[下载最新版 TypeStatus（Windows x64）](https://github.com/Fab1e2000/TypeStatus/releases/latest/download/TypeStatus-windows-x64.zip)

[查看版本说明与历史版本](https://github.com/Fab1e2000/TypeStatus/releases)

- 支持 Windows 10、Windows 11，x64
- 绿色软件，无需安装
- 无需管理员权限
- 无需另外安装 Visual C++ Redistributable

## 快速开始

1. 下载并解压 `TypeStatus-windows-x64.zip`。
2. 双击运行 `TypeStatus.exe`。
3. 程序启动后不会显示主窗口，请在任务栏右侧的系统托盘中寻找 TypeStatus 图标。
4. 切换输入法的中文或英文模式，观察鼠标箭头和文本 I-beam 光标的颜色。
5. 右键托盘图标，可以查看状态、暂停/继续着色或退出程序。

正常退出或暂停时，TypeStatus 会恢复当前 Windows 光标方案。

更完整的操作说明请参阅[用户指南](docs/用户指南.md)。

## 已验证的输入法

- Windows 自带英文键盘
- 微信输入法：中文模式、英文模式
- 搜狗输入法：中文模式、英文模式
- 百度输入法：中文模式、英文模式

微软拼音及其他输入法仍需进一步实机验证。TypeStatus 使用 Windows 通用输入法接口，但不同输入法的实现可能存在差异。

## 隐私与安全

TypeStatus：

- 不读取或保存输入内容
- 不监听或记录按键
- 不连接网络
- 不注入其他应用进程
- 不需要管理员权限
- 只读取当前焦点线程的键盘布局和 IME 状态

## 使用须知

- TypeStatus 修改的是当前 Windows 用户会话中的标准箭头和 I-beam 光标，因此颜色变化不只作用于某一个软件。
- 使用自绘光标的应用、游戏和部分 Chromium/Electron 控件可能不会变色。
- 普通权限程序可能无法读取管理员权限应用的输入法状态；此时会恢复 Windows 默认光标。
- 状态默认每 200 毫秒检查一次，因此颜色变化可能有短暂延迟。

### 异常退出后光标没有恢复

依次尝试：

1. 重新运行 TypeStatus，再从托盘菜单选择“退出”。
2. 打开 Windows“鼠标属性”的“指针”页面，重新应用当前指针方案。
3. 注销或重新启动 Windows。

TypeStatus 不会修改原始 `.cur` 光标文件，也不会改写你的鼠标方案文件。

## 反馈问题

请在 [GitHub Issues](https://github.com/Fab1e2000/TypeStatus/issues) 提交问题，并尽量提供：

- Windows 版本
- 输入法名称和版本
- 出现问题的应用名称
- 实际输入状态与显示颜色
- 目标应用是否以管理员身份运行
- 问题能否稳定复现

请勿提交任何敏感输入内容。

<details>
<summary><strong>面向开发者：构建与实现说明</strong></summary>

### 技术方案

- C++20 与 Win32 API
- CMake 3.24+
- Visual Studio Build Tools 2022
- 静态 MSVC 运行库

程序每 200 毫秒读取一次前台焦点线程的键盘布局。对于 IME 布局，通过 `ImmGetDefaultIMEWnd` 和带超时的 `SendMessageTimeout(WM_IME_CONTROL)` 查询打开状态及转换模式。只有检测结果发生变化时才调用 `SetSystemCursor`。

### 构建

在 Developer PowerShell for VS 2022 中执行：

```powershell
cmake --preset vs2022-x64
cmake --build --preset release
```

生成文件位于：

```text
build\vs2022-x64\Release\TypeStatus.exe
```

### 项目结构

```text
src/
  input_mode.*       前台焦点、键盘布局与 IME 状态检测
  cursor_renderer.*  系统光标取样、着色、应用与恢复
  app.*              隐藏窗口、定时器和系统托盘
  main.cpp           单实例入口
resources/           图标、Manifest 与版本资源
docs/用户指南.md      面向普通用户的使用文档
scripts/             原型和资源监测脚本
```

### 持续集成

推送分支、Pull Request 或 `v*` 标签时，GitHub Actions 会在 Windows 环境构建 Release 配置并上传 ZIP 构建产物。正式 GitHub Release 由维护者检查构建包后发布。

</details>

## 许可证

本项目采用 [MIT License](LICENSE)。
