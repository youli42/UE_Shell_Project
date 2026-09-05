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
class UShellTerminalWidget;
class UShellWorldScreen;
class UWidgetComponent;
struct FInputActionValue;

/** 手持屏幕的世界姿态：决定世界屏相对相机的位置/大小（用于面前↔手持动画）。 */
UENUM(BlueprintType)
enum class EShellScreenPose : uint8
{
	Front  UMETA(DisplayName = "Front (面前)"),   // 举到面前：近/居中/大（输入/阅读）
	Hand   UMETA(DisplayName = "Hand (手持)"),    // 放低到手：前/左/下/小
};

/**
 * 第一人称角色（GAS 示范）：ASC 由 PlayerState 持有并转发，
 * PossessedBy 时把 Avatar 切到本角色。视觉用引擎基础方块。
 *
 * 手持屏幕统一走 UShellWorldScreen 组件（显示+交互+输入接管一套接口）：
 *  - 面片承载 UShellTerminalWidget，挂在相机下，在"面前(输入)"与"手持"两姿态间插值动画；
 *  - 前置态可点击/滚轮（组件自包含指针输入，由 PlayerController 的 InputWindow 态激活）。
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

	/** 世界屏幕组件（统一显示/交互/输入接管接口）。 */
	UShellWorldScreen* GetWorldScreen() const { return WorldScreen; }

	/** 世界实例载体组件（供外部驱动 transform）。 */
	UWidgetComponent* GetShellScreenComponent() const;

	/** 世界实例上的终端 Widget（若已初始化）。 */
	UShellTerminalWidget* GetShellScreenWidget() const;

	/** 设置屏幕目标姿态（面前/手持）；Tick 内插值动画到该姿态。 */
	void SetShellScreenPose(EShellScreenPose InPose);

	/** 输入接管查询：true 时角色忽略移动/视角（把输入让给 UI 交互）。 */
	bool IsShellInputActive() const;

private:
	/** PlayerState 侧 ASC 的 Avatar 绑定（客户端/本地共用）。 */
	void BindAbilityAvatar();

	/** 世界屏幕朝向相机 + 手持倾角。 */
	void UpdateShellScreenBillboard();

	/** 把世界屏相对位置/物理尺寸向目标姿态插值。 */
	void InterpShellScreenToPose(float DeltaSeconds);

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

	/** 双实例-世界实例：手持屏幕（UShellWorldScreen 统一组件）。 */
	UPROPERTY(VisibleAnywhere, Category = "Shell Project|Shell Screen")
	TObjectPtr<UShellWorldScreen> WorldScreen;

	/** 当前目标姿态（面前/手持），Tick 插值动画用。 */
	EShellScreenPose ShellPose = EShellScreenPose::Front;

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
