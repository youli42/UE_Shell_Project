#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"

#include "ShellProjectCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputComponent;
class UInputMappingContext;
class UStaticMeshComponent;
class UWidgetComponent;
class UShellTerminalWidget;
struct FInputActionValue;

/**
 * 第一人称角色（GAS 示范）：ASC 由 PlayerState 持有并转发，
 * PossessedBy 时把 Avatar 切到本角色。视觉用引擎基础方块。
 *
 * T12 演示前缀：
 *  - 角色挂一个"手持屏幕"世界实例（UWidgetComponent 承载 UShellTerminalWidget），
 *    用于双实例共存模式的"倾斜如拿在手里"状态。
 *  - **第一人称**移动/视角：WASD（相机相对）+ 鼠标视角 + 空格跳跃。
 *  - 输入**程序化构建**（NewObject 建动作+IMC），不依赖 /Game/Input 资产，
 *    规避 IMC 资产保存崩溃；1D 动作无 swizzle 歧义。
 */
UCLASS()
class UE_SHELL_PROJECT_API AShellProjectCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AShellProjectCharacter();

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** 每帧 billboard：让屏幕世界实例始终以"手持倾角"面向玩家相机。 */
	virtual void Tick(float DeltaSeconds) override;

	/** 增强输入绑定挂载点（WASD/鼠标/空格）。 */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void BeginPlay() override;

	/** 世界实例组件（供控制器切换可见性）。 */
	UWidgetComponent* GetShellScreenComponent() const { return ShellScreen; }

	/** 世界实例上的终端 Widget（若已初始化）。 */
	UShellTerminalWidget* GetShellScreenWidget() const;

private:
	/** PlayerState 侧 ASC 的 Avatar 绑定（客户端/本地共用）。 */
	void BindAbilityAvatar();

	/** 世界屏幕朝向相机 + 手持倾角。 */
	void UpdateShellScreenBillboard();

	/** 程序化构建动作+IMC，并挂到本地玩家的增强输入子系统。幂等。 */
	void BuildAndAddCharacterInputMapping();

	/** 惰性创建动作+IMC（须在运行时调用，不能在构造器 NewObject）。幂等。 */
	void EnsureInputBuilt();

	/** W/S：前/后（IA 为 1D，+前 -后）。 */
	void MoveForward(const struct FInputActionValue& Value);

	/** A/D：左/右。 */
	void MoveRight(const struct FInputActionValue& Value);

	/** 鼠标视角（X=偏航, Y=俯仰）。 */
	void Look(const struct FInputActionValue& Value);

	/** 空格跳跃。 */
	void HandleJump();
	void HandleJumpReleased();

	UPROPERTY(VisibleAnywhere, Category = "Shell Project")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, Category = "Shell Project")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** 双实例-世界实例：手持屏幕。归属角色根，面向相机 billboard。 */
	UPROPERTY(VisibleAnywhere, Category = "Shell Project|Shell Screen")
	TObjectPtr<UWidgetComponent> ShellScreen;

	/** 增强输入（运行时 NewObject 构建，UPROPERTY 防 GC）。 */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> CharacterMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveForwardAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveRightAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> JumpAction;
};
