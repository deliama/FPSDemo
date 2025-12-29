#pragma once

#include "CoreMinimal.h"
#include "ShooterTypes.generated.h" // 这一行很重要，文件名必须对应

/** * 存放玩家数据的公共结构体
 * 将其放在单独的文件中，供 GameMode 和 UI 共同使用，避免重名冲突
 */
USTRUCT(BlueprintType)
struct FPlayerStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<APlayerController> PlayerController = nullptr;

	UPROPERTY(BlueprintReadOnly)
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Deaths = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Assists = 0;

	FPlayerStats() {}

	FPlayerStats(APlayerController* InPC)
		: PlayerController(InPC), Kills(0), Deaths(0), Assists(0) {}
};