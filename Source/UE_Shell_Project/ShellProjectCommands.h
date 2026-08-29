#pragma once

#include "CoreMinimal.h"

class FShellCommandRegistry;

/**
 * 宿主自定义 shell 命令（接入演示）。
 *
 * 演示 C++ 注册 API：`hello [name]`（可选 String 参数、无需登录）
 * 与 `roll <sides>`（必填 Int 参数、需登录）。
 * 注册入口见 ShellMenuGameMode.cpp —— 菜单关卡拿到 Shell 子系统后调用一次。
 */
namespace ShellProjectCommands
{
	/** 注册宿主自定义命令（幂等：static 保护，可重复调用）。 */
	void RegisterProjectCommands(FShellCommandRegistry& InRegistry);
}
