#include "NinjaTrainingHUD.h"
#include "NinjaCharacter.h"
#include "Engine/Canvas.h"

void ANinjaTrainingHUD::DrawHUD()
{
	Super::DrawHUD();
	const auto* Ninja = Cast<ANinjaCharacter>(GetOwningPawn());
	if (!Ninja || !Canvas) return;
	const float Scale = FMath::Clamp(Canvas->ClipX / 1280.f, 0.75f, 1.5f);
	const float X = 28 * Scale;
	DrawRect(FLinearColor(0.015f, 0.025f, 0.045f, 0.88f), 16 * Scale, 16 * Scale, 520 * Scale, 258 * Scale);
	const bool bCity = GetWorld()->GetMapName().Contains(TEXT("L_Ikebukuro"));
	const bool bCombat=GetWorld()->GetMapName().Contains(TEXT("L_NinjaCombat"));
	DrawText(bCity ? TEXT("IKEBUKURO / NINJA") : bCombat ? TEXT("NINJA / COMBAT TRAINING") : TEXT("NINJA / MOVEMENT TRAINING"), FLinearColor::White, X, 26 * Scale, nullptr, 1.2f * Scale);
	DrawText(TEXT("WASD move  |  SHIFT run  |  SPACE jump again near apex"), FLinearColor(0.8f, 0.85f, 0.9f), X, 52 * Scale, nullptr, Scale);
	DrawText(FString::Printf(TEXT("JUMP %d / 3    %s    %.1f m/s"), Ninja->JumpStage,
		Ninja->bIsSprinting ? TEXT("RUN") : TEXT("WALK"), Ninja->GetVelocity().Size2D() / 100),
		FLinearColor::White, X, 79 * Scale, nullptr, 1.3f * Scale);
	const bool bWindow = Ninja->CanChainJump();
	DrawText(bWindow ? TEXT("SPACE NOW!") : Ninja->JumpFeedback,
		bWindow ? FLinearColor(0.2f, 1, 0.55f) : FLinearColor(1, 0.8f, 0.35f), X, 109 * Scale, nullptr, 1.25f * Scale);
	const float Width = 420 * Scale;
	DrawRect(FLinearColor(0.12f, 0.16f, 0.22f), X, 150 * Scale, Width, 12 * Scale);
	DrawRect(FLinearColor(0.1f, 0.55f, 0.3f), X + Width * 0.4f, 150 * Scale, Width * 0.2f, 12 * Scale);
	if (Ninja->JumpStage > 0 && Ninja->JumpStage < 3)
	{
		const float Position = FMath::Clamp(0.5f - Ninja->GetSecondsToJumpApex() / (Ninja->TimingHalfWindow * 10), 0.f, 1.f);
		DrawRect(FLinearColor::White, X + Width * Position - 2, 146 * Scale, 4, 20 * Scale);
	}
	DrawText(bCity ? TEXT("SPACE near apex  |  R: return to the safe street") : TEXT("Press again when the marker reaches the green zone"), FLinearColor(0.7f, 0.8f, 0.85f), X, 171 * Scale, nullptr, Scale);
	DrawText(FString::Printf(TEXT("HP %d / 3%s   DEFEATED %d"),Ninja->Health,Ninja->IsInvulnerable()?TEXT(" [PROTECTED]"):TEXT(""),Ninja->Defeats),FLinearColor(1,.7f,.4f),X,196*Scale,nullptr,1.2f*Scale);
	DrawText(TEXT("LMB / F: throw   |   R: return   |   Stomp from above"),FLinearColor::White,X,222*Scale,nullptr,Scale);
	DrawText(Ninja->CombatFeedback,FLinearColor(.65f,.9f,1),X,244*Scale,nullptr,.9f*Scale);
	const float CX=Canvas->ClipX/2,CY=Canvas->ClipY/2;
	DrawLine(CX-8,CY,CX-3,CY,FLinearColor::White,1.5f);
	DrawLine(CX+3,CY,CX+8,CY,FLinearColor::White,1.5f);
	DrawLine(CX,CY-8,CX,CY-3,FLinearColor::White,1.5f);
	DrawLine(CX,CY+3,CX,CY+8,FLinearColor::White,1.5f);
}
