// FBlasterTokenBucket 实现：令牌桶补发与消费算法

#include "TokenBucket.h"

void FBlasterTokenBucket::Reset()
{
	Tokens = Capacity;
	LastRefillTime = -1.f;
}

bool FBlasterTokenBucket::TryConsume(float ServerTimeNow)
{
	if (LastRefillTime < 0.f)
	{
		// 首次使用：只建立基准时间并回满，不把"开局前经过的时间"折成令牌
		LastRefillTime = ServerTimeNow;
		Tokens = Capacity;
	}
	else
	{
		const float Elapsed = FMath::Max(0.f, ServerTimeNow - LastRefillTime);
		if (Elapsed > 0.f)
		{
			// 按流逝时间补发；Min 封顶到容量，服务端卡顿后不会瞬时产生海量令牌
			Tokens = FMath::Min(Capacity, Tokens + Elapsed * RefillRatePerSecond);
			// 仅在确实补发后推进基准：同帧多次调用 Elapsed==0，不吞时间片
			LastRefillTime = ServerTimeNow;
		}
	}

	// 浮点误差容差：补发后 0.9999999 也能消费
	if (Tokens >= 1.f - KINDA_SMALL_NUMBER)
	{
		Tokens -= 1.f;
		return true;
	}
	return false;
}
