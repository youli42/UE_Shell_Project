#include "ShellProjectCommands.h"

#include "Math/UnrealMathUtility.h"
#include "Shell/Terminal/ShellCommand.h"
#include "Shell/Terminal/ShellCommandRegistry.h"
#include "Shell/Terminal/ShellSubsystem.h"
#include "Shell/Terminal/ShellTypes.h"

namespace ShellProjectCommands
{
	namespace
	{
		FShellCommandResult HandleHello(const FShellCommandContext& InContext)
		{
			FShellCommandResult Result;
			UShellSubsystem* Shell = InContext.Shell;
			if (!Shell)
			{
				return Result;
			}

			const FString Name = InContext.Args.IsValidIndex(0) ? InContext.Args[0] : TEXT("shell user");
			Shell->Print(FString::Printf(TEXT("Hello, %s! This command was registered by the host project."), *Name), EShellOutputType::Success);
			Result.bSuccess = true;
			return Result;
		}

		FShellCommandResult HandleRoll(const FShellCommandContext& InContext)
		{
			FShellCommandResult Result;
			UShellSubsystem* Shell = InContext.Shell;
			if (!Shell)
			{
				return Result;
			}

			// 必填 Int 参数已由 ValidateArgs 校验通过，这里直接解析。
			const int32 Sides = FMath::Max(1, FCString::Atoi(*InContext.Args[0]));
			const int32 Roll = FMath::RandRange(1, Sides);
			Shell->Print(FString::Printf(TEXT("You rolled a %d (d%d)"), Roll, Sides), EShellOutputType::Success);
			Result.bSuccess = true;
			return Result;
		}
	} // namespace

	void RegisterProjectCommands(FShellCommandRegistry& InRegistry)
	{
		// 注册表挂在 GameInstance 子系统上跨关卡存活；菜单 GameMode 每次进菜单
		// 会重建，static 保证整进程只注册一次（RegisterCommand 重名也静默拒绝）。
		static bool bRegistered = false;
		if (bRegistered)
		{
			return;
		}
		bRegistered = true;

		FShellCommand Hello;
		Hello.Name = TEXT("hello");
		Hello.Category = TEXT("Host Demo");
		Hello.Help = TEXT("hello [name] - greet the shell (host-registered example)");
		FShellArgSpec NameArg;
		NameArg.Name = TEXT("name");
		NameArg.Type = EShellArgType::String; // 可选参数：bRequired 默认 false
		Hello.Args.Add(NameArg);
		Hello.Handler = HandleHello;
		InRegistry.RegisterCommand(Hello);

		FShellCommand Roll;
		Roll.Name = TEXT("roll");
		Roll.Category = TEXT("Host Demo");
		Roll.Help = TEXT("roll <sides> - roll a die (host-registered, requires login)");
		Roll.bRequiresLogin = true; // 未登录执行会被注册表拒绝
		FShellArgSpec SidesArg;
		SidesArg.Name = TEXT("sides");
		SidesArg.bRequired = true;
		SidesArg.Type = EShellArgType::Int;
		Roll.Args.Add(SidesArg);
		Roll.Handler = HandleRoll;
		InRegistry.RegisterCommand(Roll);
	}
} // namespace ShellProjectCommands
