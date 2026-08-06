# TypeStatus

TypeStatus 是一个 Windows 开源小工具。它根据当前输入状态改变系统鼠标光标，让你不看任务栏也能判断下一次输入是中文还是英文。

- 中文输入：红色 I-beam，普通箭头的深色描边同步变红
- 英文输入：蓝色 I-beam，普通箭头的深色描边同步变蓝
- 无法判断：恢复当前 Windows 光标方案

项目当前处于 **MVP（0.1.1）** 阶段。Windows 英文键盘，以及微信、搜狗、百度输入法的中英文模式已经通过实测；其他输入法仍需逐项验证，暂不作兼容承诺。

普通用户请阅读[用户指南](docs/用户指南.md)。

## 实现原则

- 原生 C++20 与 Win32 API，不依赖 .NET 运行时
- 不读取键入内容，不记录按键
- 不注入 DLL，不挂钩其他进程
- 仅在输入状态发生变化时替换系统光标
- 退出、暂停及 Windows 会话结束时恢复系统光标方案
- 单实例运行，提供系统托盘入口

## 构建要求

- Windows 10 或 Windows 11，x64
- Visual Studio Build Tools 2022（无需安装完整 Visual Studio）
- Build Tools 中的“使用 C++ 的桌面开发”工作负载
- CMake 3.24 或更高版本
- Visual Studio Code
- VS Code 扩展：C/C++、CMake Tools

> VS Code 是编辑器和调试前端，本身不包含 MSVC 编译器。安装 Build Tools 时至少勾选 MSVC x64/x86、Windows 10/11 SDK 和 C++ CMake tools。

## 在 VS Code 中开发

1. 用 VS Code 打开项目根目录，而不是单独打开某个 `.cpp` 文件。
2. 接受工作区推荐的 `C/C++` 与 `CMake Tools` 扩展。
3. 重新打开 VS Code，状态栏的 CMake Preset 选择 `vs2022-x64`。
4. 按 `Ctrl+Shift+B` 构建 Debug 版本。
5. 按 `F5` 启动并调试 `TypeStatus.exe`。

工作区已经提供：

- `.vscode/tasks.json`：Configure、Debug 和 Release 构建任务；
- `.vscode/launch.json`：使用 `cppvsdbg` 调试后台 GUI 程序；
- `.vscode/settings.json`：让 CMake Tools 使用项目 Presets；
- `.vscode/extensions.json`：扩展推荐。

Debug 程序位于：

```text
build\vs2022-x64\Debug\TypeStatus.exe
```

## 命令行构建

在“Developer PowerShell for VS 2022”中执行：

```powershell
cmake --preset vs2022-x64
cmake --build --preset release
```

生成文件位于：

```text
build\vs2022-x64\Release\TypeStatus.exe
```

也可以不使用预设：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Release 配置使用静态 MSVC 运行库，发布包无需单独安装 Visual C++ Redistributable。

## 项目结构

```text
src/
  input_mode.*       前台焦点、HKL 与 IME 状态检测
  cursor_renderer.*  系统光标取样、着色、应用与恢复
  app.*              隐藏窗口、定时器和系统托盘
  main.cpp           单实例入口
resources/           Manifest 与版本资源
docs/用户指南.md      面向普通用户的使用文档
scripts/             已验证原型和资源监测脚本
```

## 检测逻辑

1. 读取前台窗口及其焦点窗口所属线程。
2. 通过 `GetKeyboardLayout` 获取该线程的 HKL；英文族键盘直接判定为英文。
3. 对 IME 布局，通过 `ImmGetDefaultIMEWnd` 获取默认 IME 窗口。
4. 使用带 100 ms 超时的 `SendMessageTimeout(WM_IME_CONTROL)` 查询打开状态和转换模式。
5. `IME_CMODE_NATIVE` 开启判定为中文，关闭判定为英文；查询失败则恢复系统默认光标。

应用每 200 ms 采样一次，但只在结果变化时调用 `SetSystemCursor`。该实现不会根据 Shift、Ctrl+Space 等按键进行猜测。

## 当前边界

- `SetSystemCursor` 修改的是当前用户会话的系统光标映射，因此颜色会影响所有使用标准箭头或 I-beam 的应用。
- 自绘光标、游戏光标和部分 Chromium/Electron 控件可能不使用系统标准光标，因而不会变化。
- 普通权限进程可能无法查询管理员权限应用中的 IME 状态；此时会回退为系统默认光标。
- 程序异常终止时来不及执行恢复逻辑，光标可能暂时保持着色。重新启动后从托盘退出，或在 Windows 鼠标设置中重新应用光标方案即可恢复。
- 目前已验证 Windows 英文键盘，以及微信、搜狗、百度输入法的中英文模式。微软拼音及其他输入法仍需在真实环境中补充兼容性测试。

## 发布流程

推送普通分支、PR 或版本标签时，GitHub Actions 会在 Windows 环境构建 Release，并上传只读的构建产物。工作流不会获得仓库写权限，也不会自动发布 GitHub Release；维护者应在检查产物后手动发布。

建议发布步骤：

1. 更新 `project(... VERSION ...)`、版本资源和变更记录。
2. 在 Windows 10/11 上完成兼容性与退出恢复测试。
3. 创建并推送版本标签，例如：`git tag v0.1.1`、`git push origin v0.1.1`。
4. 下载并检查 Actions 生成的 ZIP。
5. 在 GitHub 上手动创建 Release 并上传验证过的 ZIP。

## 许可证

本项目采用 [MIT License](LICENSE)。
