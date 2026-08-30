# Shell_UE 宿主接入指南

> 面向要把 Shell_UE 插件接入自己游戏项目的宿主开发者。
> 活示例即本仓库壳工程 `UE_Shell_Project`：本仓库的 `Source/UE_Shell_Project/`
> 下的接线代码可直接参考、复制、改造。
>
> 本文覆盖四个主题：**① 如何在项目里调用 Shell**、**② 注册你自己的命令**、
> **③ 把 Shell 作为开始页面（终端即登录页）**、**④ 调用你的 GAS 能力**。

---

## 目录

1. [概览：插件是什么，壳工程对应关系](#1-概览)
2. [第一步：把插件装进你的项目](#2-第一步把插件装进你的项目)
3. [第二步：调用 Shell（最小接线）](#3-第二步调用-shell最小接线)
4. [第三步：注册你自己的命令](#4-第三步注册你自己的命令)
5. [第四步：作为开始页面](#5-第四步作为开始页面)
6. [第五步：GAS —— 先跑通自带示例，再换你的能力](#6-第五步gas--先跑通自带示例再换你的能力)
7. [排障速查](#7-排障速查)
8. [参考文件索引](#8-参考文件索引)

---

## 1. 概览

**Shell_UE** 是一个基于 UE 5.8 的**纯插件**：克隆（或复制）到任意工程的
`Plugins/Shell_UE/` 即可挂载。它实现了一个游戏内黑客风格伪终端：

- 按 **Tab** 唤出终端，输入 `help` / `ls` / `cd` / `cat` / `login` 等命令；
- `login` 交互式登录（用户名 + 掩码密码）→ 登录成功自动进入游戏关卡；
- 终端是**纯 Slate 控件**（`SShellTerminalPanel` + 瘦壳 `UShellTerminalWidget`），
  CRT 绿色荧光外观；
- 命令系统是**自定义注册表**（非控制台命令）：C++ 注册 / DataTable 数据驱动 /
  蓝图库三种扩展方式；
- 内置 GAS 命令 `cast` / `attributes` / `cooldowns` / `giveability`，按
  **数据表 + GameplayTag** 工作，不依赖宿主的 GAS 结构。

关键架构事实（写代码前必读）：

| 事实 | 说明 |
|---|---|
| 挂载点 | `UShellSubsystem`（`UGameInstanceSubsystem`，跨关卡存活） |
| 命令签名 | `TFunction<FShellCommandResult(const FShellCommandContext&)>`（冻结契约） |
| 输入开关 | Enhanced Input，默认 **Tab**（`IMC_Shell` / `IA_TerminalToggle`，资产在插件内可重绑定） |
| 登录流 | `login` → `UShellSessionSubsystem` → 成功 `ClientTravel` 到游戏关卡 |
| 插件内容挂载路径 | `/Shell_UE/Shell/...`（自洽，不依赖宿主 `/Game/`） |
| 壳工程 = 活示例 | 每个接入点都能在 `Source/UE_Shell_Project/` 找到对应文件（见[第 8 节](#8-参考文件索引)） |

> ⚠️ 插件处于快速开发阶段，API 可能变化。文档与代码不一致时**以代码为准**。

---

## 2. 第一步：把插件装进你的项目

### 2.1 方式 A：git 子模块（推荐，可随时更新）

```bash
git submodule add git@github.com:youli42/Shell_UE.git Plugins/Shell_UE
```

### 2.2 方式 B：直接复制目录

把壳工程的 `Plugins/Shell_UE/` 整个目录复制到你项目的 `Plugins/Shell_UE/`
（注意插件自带 `.git/`，复制时可剔除）。

### 2.3 启用插件

插件描述符 `Shell_UE.uplugin` 已设置 `"EnabledByDefault": true`，且自带了
`EnhancedInput`、`GameplayAbilities` 依赖，通常**不需要**在 `.uproject` 的
`Plugins` 段手动列出。重新生成工程文件后编辑器会自动加载。

> 若你的工程尚未启用 GAS，请确认 `.uproject` 或 `DefaultEngine.ini` 中已启用
> `GameplayAbilities` 插件，并在 `DefaultEngine.ini` 设置
> `AbilitySystemGlobalsClassName`（见壳工程 `Config/DefaultEngine.ini`）。

### 2.4 游戏模块 Build.cs 依赖

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
    "UMG", "Shell_UE",
    // GAS 接入（cast/attributes/cooldowns 需要）
    "GameplayAbilities", "GameplayTags", "GameplayTasks" });
PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
```

---

## 3. 第二步：调用 Shell（最小接线）

### 3.1 PlayerController：挂输入映射 + Tab 开关

核心一行调用是
`GameInstance->GetSubsystem<UShellSubsystem>()->ToggleTerminal(this)`。
完整接线直接抄壳工程 `Source/UE_Shell_Project/ShellProjectPlayerController.cpp`：

```cpp
// ShellProjectPlayerController.h 需要：#include "InputAction.h" / "InputMappingContext.h"
//   UPROPERTY() TObjectPtr<UInputAction> TerminalToggleAction;
//   UPROPERTY() TObjectPtr<UInputMappingContext> ShellMappingContext;
//   void HandleTerminalToggle();

void AMyPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EnhancedInput) { return; }

    TerminalToggleAction = LoadObject<UInputAction>(nullptr,
        TEXT("/Shell_UE/Shell/Input/IA_TerminalToggle.IA_TerminalToggle"));
    if (TerminalToggleAction)
    {
        EnhancedInput->BindAction(TerminalToggleAction, ETriggerEvent::Started,
            this, &AMyPlayerController::HandleTerminalToggle);
    }
}

void AMyPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
        {
            ShellMappingContext = LoadObject<UInputMappingContext>(nullptr,
                TEXT("/Shell_UE/Shell/Input/IMC_Shell.IMC_Shell"));
            if (ShellMappingContext)
            {
                InputSubsystem->AddMappingContext(ShellMappingContext, 0);
            }
        }
    }
}

void AMyPlayerController::HandleTerminalToggle()
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UShellSubsystem* Shell = GameInstance->GetSubsystem<UShellSubsystem>())
        {
            Shell->ToggleTerminal(this);
        }
    }
}
```

> 头文件 include：`"Shell/Terminal/ShellSubsystem.h"`、
> `"EnhancedInputComponent.h"`、`"EnhancedInputSubsystems.h"`。

### 3.2 关键 API（UShellSubsystem，均 BlueprintCallable）

| API | 用途 |
|---|---|
| `ToggleTerminal(APlayerController*)` | 开/关终端（PC 可空，自动取本地 PC） |
| `SubmitTerminalLine(Line)` | **以用户身份敲入一行**：走 UI 提交路径，含提示符回显；终端未开时退化为纯执行 |
| `SubmitShellPrompt(Line)` | 应答活动交互提示（如登录密码） |
| `ExecuteShellCommand(Line)` | 纯执行（无回显），适合程序化调用/测试 |
| `CloseTerminal()` / `IsTerminalOpen()` | 关闭终端 / 查询状态 |
| `Print(Text, EShellOutputType)` | 向终端打印一行（Info/Success/Error/Input/System） |
| `GetRegistry()` | 返回 `FShellCommandRegistry&`，注册自定义命令的挂载点 |
| `OnShellOutput` | 输出流动态委托（`FShellOutputLine`），供宿主 UI/日志订阅 |

程序化调用示例（C++ 或蓝图）：

```cpp
UShellSubsystem* Shell = GetGameInstance()->GetSubsystem<UShellSubsystem>();
Shell->ExecuteShellCommand(TEXT("help"));        // 纯执行，无回显
Shell->SubmitTerminalLine(TEXT("login"));        // 模拟用户敲入，带回显
Shell->Print(TEXT("mission started"), EShellOutputType::Success);
```

---

## 4. 第三步：注册你自己的命令

命令注册表 `FShellCommandRegistry` 大小写不敏感，重名**先注册者生效**。
**注册时机**：`UShellSubsystem` 是 GameInstance 子系统，在 GameInstance 创建时
初始化（内部会 `Clear()` 后注册内置命令），因此宿主在任何 GameMode /
PlayerController 的 `BeginPlay` 里注册即可，之后跨关卡一直存活。

### 4.1 方式一：C++ 直接注册（核心，推荐）

`FShellCommand` 字段：

| 字段 | 说明 |
|---|---|
| `Name` | 命令名 |
| `Aliases` | 别名数组 |
| `Category` | 分组（`help` 按此分组） |
| `Help` | 帮助文本 |
| `Args` | `TArray<FShellArgSpec>` 参数规格（自动校验） |
| `bRequiresLogin` | 需要登录才能执行（未登录拒绝） |
| `bHidden` | 从 `help` 列表隐藏 |
| `Handler` | `FShellCommandHandler`，即 `TFunction<FShellCommandResult(const FShellCommandContext&)>` |

`FShellArgSpec` 参数规格：`Name` / `bRequired` / `Type`（`String|Int|Float|Bool`）。
支持位置参数与 `--Name value` 命名参数、裸 `--Flag` 布尔开关；
**校验由注册表自动执行**，Handler 里拿到的 `Args` 已通过类型校验。

`FShellCommandContext`：`Args`（命令名后的 token）、`StdinLines`（管道预留）、
`OnCompleted`（异步出口）、`Shell`（`UShellSubsystem*`，用它打印输出）。

完整示例 —— 两个命令：`hello [name]`（可选 String 参数，无需登录）和
`roll <sides>`（必填 Int 参数，需登录）：

```cpp
// ProjectCommands.h
#pragma once
#include "CoreMinimal.h"
class FShellCommandRegistry;

namespace ProjectCommands
{
    /** 注册宿主自定义命令（幂等：static 保护，可重复调用）。 */
    void RegisterProjectCommands(FShellCommandRegistry& InRegistry);
}
```

```cpp
// ProjectCommands.cpp
#include "ProjectCommands.h"

#include "Shell/Terminal/ShellCommand.h"
#include "Shell/Terminal/ShellCommandRegistry.h"
#include "Shell/Terminal/ShellSubsystem.h"
#include "Shell/Terminal/ShellTypes.h"

namespace ProjectCommands
{
    namespace
    {
        FShellCommandResult HandleHello(const FShellCommandContext& InContext)
        {
            FShellCommandResult Result;
            UShellSubsystem* Shell = InContext.Shell;
            if (!Shell) { return Result; }

            const FString Name = InContext.Args.IsValidIndex(0) ? InContext.Args[0] : TEXT("shell user");
            Shell->Print(FString::Printf(TEXT("Hello, %s!"), *Name), EShellOutputType::Success);
            Result.bSuccess = true;
            return Result;
        }

        FShellCommandResult HandleRoll(const FShellCommandContext& InContext)
        {
            FShellCommandResult Result;
            UShellSubsystem* Shell = InContext.Shell;
            if (!Shell) { return Result; }

            // 必填 Int 参数已由 ValidateArgs 校验通过，这里直接解析。
            const int32 Sides = FMath::Max(1, FCString::Atoi(*InContext.Args[0]));
            const int32 Roll = FMath::RandRange(1, Sides);
            Shell->Print(FString::Printf(TEXT("You rolled a %d (d%d)"), Roll, Sides), EShellOutputType::Success);
            Result.bSuccess = true;
            return Result;
        }
    }

    void RegisterProjectCommands(FShellCommandRegistry& InRegistry)
    {
        static bool bRegistered = false;
        if (bRegistered) { return; }   // 注册表跨关卡存活，防重复注册
        bRegistered = true;

        FShellCommand Hello;
        Hello.Name = TEXT("hello");
        Hello.Category = TEXT("Host Demo");
        Hello.Help = TEXT("hello [name] - greet the shell (host-registered example)");
        FShellArgSpec NameArg;
        NameArg.Name = TEXT("name");
        NameArg.Type = EShellArgType::String;   // 可选参数：bRequired 默认 false
        Hello.Args.Add(NameArg);
        Hello.Handler = HandleHello;
        InRegistry.RegisterCommand(Hello);

        FShellCommand Roll;
        Roll.Name = TEXT("roll");
        Roll.Category = TEXT("Host Demo");
        Roll.Help = TEXT("roll <sides> - roll a die (host-registered, requires login)");
        Roll.bRequiresLogin = true;             // 未登录执行会被注册表拒绝
        FShellArgSpec SidesArg;
        SidesArg.Name = TEXT("sides");
        SidesArg.bRequired = true;
        SidesArg.Type = EShellArgType::Int;
        Roll.Args.Add(SidesArg);
        Roll.Handler = HandleRoll;
        InRegistry.RegisterCommand(Roll);
    }
}
```

接线（在你的菜单 GameMode 里，成功拿到 Shell 后调用一次）：

```cpp
#include "ProjectCommands.h"
// ...
ProjectCommands::RegisterProjectCommands(Shell->GetRegistry());
```

> 壳工程已按此模式实现了 `hello` / `roll` 两个演示命令，见
> `Source/UE_Shell_Project/ShellProjectCommands.h/.cpp` 与其在
> `ShellMenuGameMode.cpp` 的接线。

### 4.2 方式二：DataTable 数据驱动（零编译）

插件默认从 `DT_ShellCommands`（`/Shell_UE/Shell/Data/DT_ShellCommands`）加载命令，
行类型 `FShellCommandRow`。三种执行方式 `EShellExecutionType`：

| 类型 | 行为 |
|---|---|
| `Cpp` | 重标记**已有原生命令**的元数据（Category/Help/Aliases/登录门禁/隐藏），保留原生 Handler |
| `Blueprint` | 注册**新命令**，Handler 加载指定的 `UShellCommandLibrary` 软资产，路由到其 `ExecuteCommand` 蓝图事件 |
| `Alias` | 纯别名：`CommandName` → `TargetCommand` 转发 |

配置步骤：

1. 在 Content Browser 打开（或复制）`DT_ShellCommands`；
2. 新增一行，填 `CommandName` / `Category` / `Help` / `Args` / `ExecutionType`
   （`Blueprint` 时填 `CommandLibrary` 软引用 + `EventName`，`Alias` 时填
   `TargetCommand`）/ `bRequiresLogin` / `bHidden`；
3. 保存即可，无需编译。表格缺失/行错误非致命，跳过并告警。

### 4.3 方式三：蓝图库

继承 `UShellCommandLibrary`（`UCLASS(Blueprintable)`），实现 `BlueprintNativeEvent`
`TArray<FString> ExecuteCommand(Args, bool& bSuccess)`，把资产放到
`DT_ShellCommands` 某行的 `CommandLibrary` 字段即可被调用。参考插件自带示例资产
`/Shell_UE/Shell/Libraries/LB_Hint`。

---

## 5. 第四步：作为开始页面

"终端即登录页"：游戏启动先进**菜单关卡**，自动打开终端并提交 `login`，
登录成功自动 `ClientTravel` 到**游戏关卡**。

### 5.1 地图与 GameMode（DefaultEngine.ini）

```ini
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Shell_UE/Shell/Maps/MainMenu.MainMenu
EditorStartupMap=/Shell_UE/Shell/Maps/MainMenu.MainMenu
GlobalDefaultGameMode=/Script/你的模块名.YourMenuGameMode
```

> 想用自己的地图做开始页也行：把 `GameDefaultMap` 指向你的地图，
> 再把 `UShellSettings.MenuLevelPath` / `GameplayLevelPath` 改成你的关卡路径
> （Project Settings → Shell Settings，或 `DefaultGame.ini` 的
> `[/Script/Shell_UE.ShellSettings]`）。

### 5.2 菜单关卡 GameMode：自动进入登录流

直接抄壳工程 `Source/UE_Shell_Project/ShellMenuGameMode.cpp`（幂等 + 就绪重试）：

```cpp
void AYourMenuGameMode::BeginPlay()
{
    Super::BeginPlay();
    TryStartLoginFlow();
}

void AYourMenuGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    TryStartLoginFlow();   // 幂等保护，补时序
}

void AYourMenuGameMode::TryStartLoginFlow()
{
    if (bLoginFlowStarted) { return; }

    UGameInstance* GameInstance = GetGameInstance();
    UShellSubsystem* Shell = GameInstance ? GameInstance->GetSubsystem<UShellSubsystem>() : nullptr;
    APlayerController* PC = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;

    if (!Shell || !PC)
    {
        // 下一 tick 重试（上限约 2 秒），等 PC / 子系统就绪。
        if (++LoginFlowRetryCount < 120 && GetWorld())
        {
            GetWorldTimerManager().SetTimerForNextTick(
                FTimerDelegate::CreateUObject(this, &AYourMenuGameMode::TryStartLoginFlow));
        }
        return;
    }

    bLoginFlowStarted = true;

    if (!Shell->IsTerminalOpen())
    {
        Shell->ToggleTerminal(PC);
    }
    Shell->SubmitTerminalLine(TEXT("login"));   // 以用户身份提交，走 UI 路径带回显
}
```

> 登录成功后的跳转由插件的 `login` 命令链路完成（`bAutoTravelOnLogin=true`）。
> 若你想接管跳转：在 `UShellSettings` 把 `bAutoTravelOnLogin` 置 false，
> 监听 `UShellSessionSubsystem::OnLoginSucceeded` 自行处理。

### 5.3 游戏关卡 GameMode：登录守卫 + HUD

抄壳工程 `Source/UE_Shell_Project/ShellGameplayGameMode.cpp`：

- 下一 tick 校验 `UShellSessionSubsystem::IsLoggedIn()`，未登录 `ClientTravel`
  弹回菜单；
- 已登录创建 `UShellSessionHUD`（登录态标签）加入视口，并切
  `FInputModeGameOnly`。

### 5.4 账户配置

`Accounts` 为空时回落到 demo 账户 `alice` / `alice123`。正式账户在
`DefaultGame.ini` 配置：

```ini
[/Script/Shell_UE.ShellSettings]
+Accounts=(Key="player1",Value=(Password="pass1",Role="user",DisplayName="Player One"))
```

---

## 6. 第五步：GAS —— 先跑通自带示例，再换你的能力

插件侧**不假设**宿主的 GAS 结构，按"数据表 + GameplayTag"工作。
壳工程自带一套完整可跑的示例（Fireball），先在独立项目复刻跑通，再换你自己的能力。

### 6.1 插件侧已就绪的命令（均需登录）

| 命令 | 说明 |
|---|---|
| `cast <ability>` | 按能力名激活（查表 `AbilityTag` → `TryActivateAbilitiesByTag`）；带显式反馈：`not enough mana (12/30)` / `on cooldown (2.3s)` / `activation refused` |
| `attributes` | 列出本地 ASC 已生成属性集的数值属性（按名反射读取） |
| `cooldowns` | 按能力表的 `CooldownTag` 查询生效中 GE 的剩余时间 |
| `giveability <name>` | （dev 隐藏）按表 `AbilityClass` 软类引用授予能力，需 Authority |

### 6.2 能力数据表 DT_ShellAbilities（`FShellAbilityRow`）

默认路径 `/Shell_UE/Shell/Data/DT_ShellAbilities`
（`UShellSettings.ShellAbilitiesTable` 可改）。**行键 = 玩家输入的能力名**
（大小写不敏感，如 `fireball`）：

| 字段 | 说明 |
|---|---|
| `DisplayName` | 显示名（默认取行键） |
| `AbilityTag` | 激活标签（`TryActivateAbilitiesByTag` 用） |
| `CooldownTag` | 冷却标签（查生效 GE 剩余时间；空则跳过冷却校验） |
| `ManaCost` | 法力成本（0 = 不校验；按名读取属性集里的 `Mana`/`MaxMana`） |
| `AbilityClass` | `giveability` 授予用软类引用（`cast` 本身按 Tag 激活，不直接加载） |

壳工程表内示例行：`fireball` → `AbilityTag=Ability.Skill.Fireball`、
`CooldownTag=Cooldown.Fireball`、`ManaCost=10`、`AbilityClass=UShellFireballAbility`。

### 6.3 宿主侧 GAS 结构（壳工程参考）

| 组件 | 壳工程文件 | 要点 |
|---|---|---|
| ASC | `ShellProjectPlayerState.cpp` | 构造函数 `CreateDefaultSubobject<UAbilitySystemComponent>`（Replicated、Minimal）；`BeginPlay` 里 `InitAbilityActorInfo(this, Pawn 或 this)`；Authority 侧 `GiveAbility` |
| 角色转发 | `ShellProjectCharacter` | 实现 `IAbilitySystemInterface`，`PossessedBy`/`OnRep_PlayerState` 绑定 Avatar（Owner=PlayerState） |
| 属性集 | `ShellProjectAttributeSet` | 含 `Mana`/`MaxMana`（cast 校验按名读取），`PreAttributeChange` + `PostGameplayEffectExecute` 钳制 |
| 能力 | `ShellProjectAbility` | 能力基类 + `UShellFireballAbility`（InstancedPerActor/LocalPredicted，`ActivateAbility` 走 CommitAbility → 发射投射物）；`UShellFireballCostGE`（Instant，-10 Mana）、`UShellFireballCooldownGE`（3s Duration，授予 `Cooldown.Fireball` 标签） |
| GameMode | `ShellGameplayGameMode` | **必须**设 `PlayerStateClass` 为你的自定义 PlayerState，否则 `cast` 报 no ASC |
| 标签 | `Config/DefaultGameplayTags.ini` | 声明 `Ability.Skill.Fireball`、`Cooldown.Fireball`、`Cost.Mana.Fireball` |

### 6.4 在终端验证（PIE）

```
Tab（开终端）→ login alice / alice123
cast fireball      → "fireball: cast (-10 mana)"
attributes         → [ShellProjectAttributeSet] Mana = 20.0 ...
cast fireball      → "on cooldown (2.3s)"（若冷却中）
cast fireball      → "not enough mana (0/30)"（法力耗尽时）
```

### 6.5 换成你自己的能力

1. 在 `DefaultGameplayTags.ini` 声明你的标签（如 `Ability.Skill.Lightning`）；
2. 在 `DT_ShellAbilities` 加一行：行键 = 能力名，填 `AbilityTag`（必填）、
   `CooldownTag`（可选）、`ManaCost`（可选，属性名需是 `Mana`/`MaxMana`）、
   `AbilityClass`（可选，用于 `giveability`）；
3. 确保你的能力已授予 ASC（`GiveAbility`）且 `AbilityTag` 与其
   `AbilityTags` 匹配；`cast` 用的是 `TryActivateAbilitiesByTag`，不关心
   能力类是什么；
4. 无 ASC 时检查：GameMode 的 `PlayerStateClass` 是否正确、角色是否实现
   `IAbilitySystemInterface` 并绑定 Avatar。

---

## 7. 排障速查

| 症状 | 原因 / 解法 |
|---|---|
| 终端 Tab 键无响应 | PlayerController 未挂 `IMC_Shell`，或 `InputComponent` 不是 `UEnhancedInputComponent` |
| `cast` 报 no AbilitySystemComponent | GameMode 没设 `PlayerStateClass`；或角色未实现 `IAbilitySystemInterface` 转发 |
| `cast` 报 not enough mana (0/...) | 属性名不是 `Mana`；或 ASC 未生成属性集（`InitAbilityActorInfo` 未调用） |
| 能力从不激活但返回 refused | 能力 CDO 构造太早 / `RequestGameplayTag` 拿到空标签（标签字典未加载）——用蓝图资产配置标签，或确认标签已注册后再构造 |
| 终端出现但中文乱码 | 插件自带子集化 CJK 字体，检查 `F_CJK` 字体资源是否被引用破坏 |
| 重复注册的警告 | 自定义命令注册要幂等（`static bool` 保护），注册表跨关卡存活 |

---

## 8. 参考文件索引

| 文档章节 | 壳工程参考文件 |
|---|---|
| 3 调用 Shell | `Source/UE_Shell_Project/ShellProjectPlayerController.h/.cpp` |
| 4 注册命令（C++） | `Source/UE_Shell_Project/ShellProjectCommands.h/.cpp`（`hello`/`roll` 演示）、`ShellMenuGameMode.cpp`（接线） |
| 4 注册命令（DataTable/蓝图） | `Plugins/Shell_UE/Content/Shell/Data/DT_ShellCommands`、`Plugins/Shell_UE/Content/Shell/Libraries/LB_Hint` |
| 5 开始页面 | `Source/UE_Shell_Project/ShellMenuGameMode.cpp`、`ShellGameplayGameMode.cpp`、`Config/DefaultEngine.ini` |
| 6 GAS | `Source/UE_Shell_Project/ShellProjectPlayerState.cpp`、`ShellProjectCharacter`、`ShellProjectAttributeSet`、`ShellProjectAbility`、`ShellFireballProjectile`、`Config/DefaultGameplayTags.ini` |
| 插件命令注册入口 | `Plugins/Shell_UE/Source/Shell_UE/Private/Shell/Terminal/ShellSubsystem.cpp`（`Initialize` 内 `RegisterBuiltInCommands` / `RegisterGASCommands` 等） |
| GAS 命令实现 | `Plugins/Shell_UE/Source/Shell_UE/Private/Shell/Terminal/ShellGASCommands.cpp` |
| 插件官方接入文档 | `Plugins/Shell_UE/RawContent/doc/11-宿主接入指南.md`、`Plugins/Shell_UE/README.md`、`Plugins/Shell_UE/AGENTS.md` |

---

## 9. 快捷指令 / 商店 / 脚本（2026-08-30 新增，插件 M2–M4）

详见插件文档 `Plugins/Shell_UE/RawContent/doc/12-快捷指令与脚本.md`。宿主要点：

1. **玩家快捷指令**：终端输入行上方自动出现 CRT 风格 chip 栏；数字键 1-9 触发
   （仅输入框为空且无修饰键时接管，不影响打字）。每条命令可配置
   「立即执行（与真实回车同路径）」或「仅填充输入框」。
   列表 API（`UShellSubsystem`，均 BlueprintCallable）：
   `GetQuickCommands / AddQuickCommand / RemoveQuickCommand / SetQuickCommandMode / RunQuickCommand`；
   列表变化广播 `OnQuickCommandsChanged`。持久化走 SaveGame
   （`ShellSettings.SaveSlotName/SaveUserIndex`，默认槽 `ShellPlayer`）。
   终端命令 `qcmd list / add <label> <cmd> / mode <n> <exec|fill> / rm <n>`（需登录）。
2. **商店目录**：插件自带 `/Shell_UE/Shell/Data/DT_ShellQuickCommandStore`
   （行结构 `FShellQuickCommandRow`：Label/CommandLine/Mode/Description/ScriptContent，
   行键即商店 id）。宿主替换 = Project Settings → Shell Settings → `ShellStoreTable`；
   置空 = 关闭商店。终端命令 `store list / info <id> / install <id>`（需登录，
   已装条目带 ✓）。脚本类条目（`ScriptContent` 非空）安装时写入
   `/home/scripts-store/<行键>.sh` 并生成 `sh` 命令。远程商店后续实现
   `IShellQuickCommandStore` 接口即可插入，UI/命令不动。
3. **玩家脚本**：`edit <path>` 捕获模式（后续行逐行入文件，`end` 保存，
   无参 `edit` 取消）+ `sh <path>` 运行。语法 v1：每行一个管道、`#` 注释、
   `set NAME value` 变量、`$var` 整词替换、`repeat N <管道>`、出错即停、2000 行预算。
   `/home/**` 下的写入（重定向/编辑/商店脚本）自动持久化到 SaveGame；
   宿主切换玩家档案用 `UShellSaveGameStore::Reload()` + `UShellVirtualFS::ReloadOverlay()`。
4. **管道语法**：`|`、`&&`、`;`、`>`、`>>` 对所有命令（含宿主自定义命令）生效；
   `ExecuteShellCommand` 同样支持。含这些操作符的快捷指令命令行请整体加引号。

| 功能 | 壳工程/插件参考 |
|---|---|
| 快捷指令数据与持久化 | `Plugins/Shell_UE/.../Public/Shell/Terminal/ShellQuickCommand.h`、`ShellSaveGame.h/.cpp` |
| 快捷栏 UI + 数字键 | `ShellTerminalPanel.cpp`（`RefreshQuickBar` / `HandleQuickCommandKey`） |
| 脚本执行器 | `ShellScript.h/.cpp`、`ScriptCommands.cpp`（sh/edit） |
| 商店 | `ShellQuickCommandStore.h/.cpp`、`StoreCommands.cpp`、`DT_ShellQuickCommandStore` |
| 管道执行器 | `ShellPipeline.h/.cpp`、`ShellParser.h`（token） |
