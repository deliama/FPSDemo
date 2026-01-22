// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "FPSDemoCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  基础第一人称角色类
 *  基于 UE5 官方 First Person 模板，提供基础的第一人称移动、视角控制等功能
 *  作为所有 FPS 角色类型的基类
 */
UCLASS(abstract)
class AFPSDemoCharacter : public ACharacter
{
	GENERATED_BODY()

	/** 第一人称网格组件（手臂模型，仅玩家自己可见） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** 第一人称相机组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

protected:

	/** 跳跃输入动作 */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** 移动输入动作 */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** 视角旋转输入动作（手柄/触摸） */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** 鼠标视角旋转输入动作 */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;
	
public:
	/** 构造函数，初始化第一人称角色的组件和配置 */
	AFPSDemoCharacter();

protected:

	/** 处理移动输入（由 Enhanced Input 系统调用） */
	void MoveInput(const FInputActionValue& Value);

	/** 处理视角旋转输入（由 Enhanced Input 系统调用） */
	void LookInput(const FInputActionValue& Value);

	/** 处理视角旋转（可在蓝图或 C++ 中调用，支持多平台输入） */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** 处理角色移动（可在蓝图或 C++ 中调用，支持多平台输入） */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** 开始跳跃 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** 停止跳跃 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

protected:

	/** 设置输入绑定（绑定 Enhanced Input 动作到对应的函数） */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

