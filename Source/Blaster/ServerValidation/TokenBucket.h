// FBlasterTokenBucket：可复用的令牌桶（Token Bucket）限频器
// 归属：ServerValidation/（P3"服务器端校验"主题目录）
// 用途：限流高频 Server RPC（防客户端刷包）。挂在射击 RPC 入口，超限直接 return。
//
// 语义：
//   Capacity             = 桶容量（瞬时突发上限，初始即满）
//   RefillRatePerSecond  = 补发速率（持续 RPC/秒上限）
//   TryConsume()         = 尝试消费 1 个令牌，成功返回 true
//
// 时间基准：由调用方传入服务器世界时间 GetWorld()->GetTimeSeconds()。
// 结构体不持有 UWorld，保持可复用 / 可单测。同帧多次调用不会吞掉时间片。

#pragma once

#include "CoreMinimal.h"

struct BLASTER_API FBlasterTokenBucket
{
	// 尝试消费一个令牌：先按流逝时间补发，再看是否有可用令牌
	bool TryConsume(float ServerTimeNow);
	// 重置为初始满桶状态（切武器/重生时可选调用）
	void Reset();

	// 桶容量：瞬时最多连续放行次数
	float Capacity = 3.f;
	// 补发速率：每秒恢复的令牌数（持续速率上限）
	float RefillRatePerSecond = 15.f;
	// 当前令牌数（始终被钳在 [0, Capacity]）
	float Tokens = 3.f;

private:
	// 上次补发的时间基准；-1 表示未初始化（首次使用才建基准，不把开局前时间折成令牌）
	float LastRefillTime = -1.f;
};
