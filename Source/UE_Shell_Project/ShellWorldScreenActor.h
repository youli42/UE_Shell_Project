#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ShellWorldScreenActor.generated.h"

class USceneComponent;
class UShellWorldScreen;

/**
 * 开始界面（R2）：场景面片上的终端登录屏。
 *
 * 用一个可放入关卡（MainMenu）的场景 Actor 挂 UShellWorldScreen，
 * 摆在玩家放的面片位置、朝向相机（FaceCamera / 手动摆位）。
 * 固定相机时交互走鼠标射线命中面片（InteractionSource=Mouse）+ FocusTerminal 打字。
 *
 * 作为登录/开始界面：登录流程沿用现有 login（UShellSubsystem 命令系统），
 * 本 Actor 只是把终端实例放到世界里。面片分辨率默认 1920x1080。
 */
UCLASS(Blueprintable, BlueprintType)
class UE_SHELL_PROJECT_API AShellWorldScreenActor : public AActor
{
	GENERATED_BODY()

public:
	AShellWorldScreenActor();

	/** 面片屏幕组件（供外部驱动显隐/聚焦/姿态/交互）。 */
	UFUNCTION(BlueprintPure, Category = "Shell|WorldScreen")
	UShellWorldScreen* GetWorldScreen() const { return WorldScreen; }

	/** 让面片面向玩家相机（开始界面相机固定，摆位后调用一次即可）。 */
	UFUNCTION(BlueprintCallable, Category = "Shell|WorldScreen")
	void FaceCamera();

	/** 显示并聚焦终端（开始界面激活为登录屏：可见 + 聚焦打字）。 */
	UFUNCTION(BlueprintCallable, Category = "Shell|WorldScreen")
	void ShowLoginScreen();

	/** 把 StartupBanner 输出到终端（可随时手动调用）。 */
	UFUNCTION(BlueprintCallable, Category = "Shell|WorldScreen")
	void PrintBanner();

protected:
	virtual void BeginPlay() override;

public:
	// -------------------------------------------------------------------------
	// 可配置（编辑器/蓝图）。
	// -------------------------------------------------------------------------

	/** 进入关卡后自动显示面片并聚焦终端（开始界面即登录屏）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell|WorldScreen")
	bool bAutoShowOnBeginPlay = true;

	/** 进入关卡后自动把 StartupBanner 打印到终端。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell|WorldScreen")
	bool bAutoPrintBannerOnBeginPlay = true;

	/** 开机横幅：用于说明当前状态（例如"服务器正在被攻击，请输入 login 登录"）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell|WorldScreen")
	FString StartupBanner = TEXT("服务器正在被攻击，请输入 login 登录");

	/**
	 * 进入关卡后自动让面片朝向玩家相机。
	 * 默认 false：屏幕朝向跟随 Actor 自身旋转（你摆什么角度就是什么角度）。
	 * 勾选后进入时对准相机一次（FaceCamera）；也可随时手动调用 FaceCamera()。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell|WorldScreen")
	bool bAutoFaceCameraOnBeginPlay = false;

private:
	/** 显示 + 聚焦 + 打印横幅（BeginPlay 走 SetTimerForNextTick 以确保终端已订阅输出）。 */
	void ActivateLoginScreen();

	UPROPERTY(VisibleAnywhere, Category = "Shell|WorldScreen")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Shell|WorldScreen")
	TObjectPtr<UShellWorldScreen> WorldScreen;
};
