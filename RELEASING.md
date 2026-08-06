# TypeStatus 发布检查清单

## 自动检查

```powershell
cmake --preset vs2022-x64
cmake --build --preset release
ctest --test-dir build/vs2022-x64 -C Release --output-on-failure
./scripts/test-runtime.ps1 -ExecutablePath build/vs2022-x64/Release/TypeStatus.exe
```

运行时测试会暂时修改 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 中的 TypeStatus 值，结束时恢复原值；执行前不能有 TypeStatus 实例正在运行。

## 人工回归

- 检查 Windows 10、Windows 11，以及 100%、150%、200% 显示缩放。
- 检查白色、黑色、反色、自定义和大尺寸光标方案。
- 检查微软拼音、微信、搜狗和百度输入法的中英文模式。
- 检查普通权限与管理员权限应用、Explorer 重启、睡眠唤醒、注销登录和开机自启。
- 运行 8–24 小时资源测试，确认内存、句柄和 GDI/USER 对象没有持续增长。

## 签名与打包

正式 1.0 构建必须使用可信 Authenticode 证书签名。将 PFX 密码放入 `TYPESTATUS_SIGNING_PASSWORD` 环境变量，然后执行：

```powershell
./scripts/sign-release.ps1 `
  -ExecutablePath build/vs2022-x64/Release/TypeStatus.exe `
  -CertificatePath C:/secure/typestatus-signing.pfx
```

GitHub Actions 使用 `SIGNING_CERTIFICATE_BASE64` 和 `TYPESTATUS_SIGNING_PASSWORD` 两个仓库 Secret。没有配置证书时仍会构建测试包，但正式 1.0 发布前必须检查签名状态为 `Valid`。

```powershell
./scripts/package-release.ps1 `
  -ExecutablePath build/vs2022-x64/Release/TypeStatus.exe `
  -OutputDirectory dist/v1.0.0 `
  -RequireSignature
```

发布前确认 ZIP 内含 EXE、用户指南、更新日志和许可证，并核对生成的 SHA-256 文件。
