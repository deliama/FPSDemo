// Copyright Epic Games, Inc. All Rights Reserved.

#include "FPSDemoCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "FPSDemo.h"

AFPSDemoCharacter::AFPSDemoCharacter()
{
	// 设置碰撞胶囊体大小
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// 创建第一人称网格（手臂模型，仅玩家自己可见）
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);  // 仅拥有者可见
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));  // 无碰撞

	// 创建第一人称相机组件，附加到手臂网格的头部插槽
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;  // 使用 Pawn 控制旋转
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;  // 启用第一人称 FOV
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;  // 启用第一人称缩放
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;  // 第一人称视野角度
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;  // 第一人称缩放比例

	// 配置第三人称网格（其他玩家看到的身体模型）
	GetMesh()->SetOwnerNoSee(true);  // 拥有者不可见（避免手臂和身体重叠）
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	// 重新设置胶囊体大小以匹配角色网格
	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// 配置角色移动组件
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;  // 下落时的减速度
	GetCharacterMovement()->AirControl = 0.5f;  // 空中控制能力（0-1）
}

void AFPSDemoCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// 设置 Enhanced Input 动作绑定
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// 绑定跳跃：按下时开始跳跃，释放时停止跳跃
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AFPSDemoCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AFPSDemoCharacter::DoJumpEnd);

		// 绑定移动：持续触发（手柄摇杆/键盘方向键）
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AFPSDemoCharacter::MoveInput);

		// 绑定视角旋转：手柄和鼠标都使用相同处理函数
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AFPSDemoCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AFPSDemoCharacter::LookInput);
	}
	else
	{
		UE_LOG(LogFPSDemo, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

/** 处理移动输入，将输入值转换为角色移动 */
void AFPSDemoCharacter::MoveInput(const FInputActionValue& Value)
{
	// 获取 2D 移动向量（X: 左右，Y: 前后）
	FVector2D MovementVector = Value.Get<FVector2D>();

	// 将移动输入传递给处理函数
	DoMove(MovementVector.X, MovementVector.Y);
}

/** 处理视角旋转输入，将输入值转换为相机旋转 */
void AFPSDemoCharacter::LookInput(const FInputActionValue& Value)
{
	// 获取 2D 旋转向量（X: 左右旋转，Y: 上下旋转）
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// 将旋转输入传递给处理函数
	DoAim(LookAxisVector.X, LookAxisVector.Y);
}

/** 处理视角旋转（Yaw: 左右，Pitch: 上下） */
void AFPSDemoCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// 应用水平旋转（左右转头）
		AddControllerYawInput(Yaw);
		// 应用垂直旋转（上下抬头/低头）
		AddControllerPitchInput(Pitch);
	}
}

/** 处理角色移动（Right: 左右，Forward: 前后） */
void AFPSDemoCharacter::DoMove(float Right, float Forward)
{
	if (GetController())
	{
		// 沿角色右侧向量移动（左右）
		AddMovementInput(GetActorRightVector(), Right);
		// 沿角色前方向量移动（前后）
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

/** 开始跳跃 */
void AFPSDemoCharacter::DoJumpStart()
{
	Jump();
}

/** 停止跳跃 */
void AFPSDemoCharacter::DoJumpEnd()
{
	StopJumping();
}
