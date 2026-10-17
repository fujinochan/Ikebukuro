#include "NinjaTrainingHUD.h"
#include "NinjaCharacter.h"
#include "Engine/Canvas.h"
#include "RedNinja.h"
#include "NinjaMatchDirector.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

void ANinjaTrainingHUD::DrawHUD()
{
	Super::DrawHUD();
	const auto* Ninja = Cast<ANinjaCharacter>(GetOwningPawn());
	if (!Ninja || !Canvas) return;
	auto* Match=ANinjaMatchDirector::Find(this);
	if (Match && (Match->State!=ENinjaMatchState::Playing || UGameplayStatics::IsGamePaused(this))) { DrawMatchHUD(Match); return; }
	const float Scale = FMath::Clamp(Canvas->ClipX / 1280.f, 0.75f, 1.5f);
	const float X = 28 * Scale;
	const bool bCity = GetWorld()->GetMapName().Contains(TEXT("L_Ikebukuro"));
	const bool bCombat=GetWorld()->GetMapName().Contains(TEXT("L_NinjaCombat"));
	const bool bEncounter=GetWorld()->GetMapName().Contains(TEXT("L_NinjaEncounter"));
	DrawRect(FLinearColor(0.015f, 0.025f, 0.045f, 0.88f), 16 * Scale, 16 * Scale, 520 * Scale, (bCity || bEncounter ? 314 : 258) * Scale);
	DrawText(bCity ? TEXT("IKEBUKURO / NINJA") : bEncounter ? TEXT("NINJA / THREE ENEMY ENCOUNTER") : bCombat ? TEXT("NINJA / COMBAT TRAINING") : TEXT("NINJA / MOVEMENT TRAINING"), FLinearColor::White, X, 26 * Scale, nullptr, 1.2f * Scale);
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
	DrawText(FString::Printf(TEXT("ONE HIT = GAME OVER   DEFEATED %d"),Ninja->Defeats),FLinearColor(1,.7f,.4f),X,196*Scale,nullptr,1.2f*Scale);
	DrawText(TEXT("LMB / F: throw   |   R: return   |   Stomp from above"),FLinearColor::White,X,222*Scale,nullptr,Scale);
	DrawText(Ninja->CombatFeedback,FLinearColor(.65f,.9f,1),X,244*Scale,nullptr,.9f*Scale);
	const float CX=Canvas->ClipX/2,CY=Canvas->ClipY/2;
	DrawLine(CX-8,CY,CX-3,CY,FLinearColor::White,1.5f);
	DrawLine(CX+3,CY,CX+8,CY,FLinearColor::White,1.5f);
	DrawLine(CX,CY-8,CX,CY-3,FLinearColor::White,1.5f);
	DrawLine(CX,CY+3,CX,CY+8,FLinearColor::White,1.5f);
	if ((bCity || bEncounter) && !Match)
	{
		int32 Alive=0;
		FVector View; FRotator Look; GetOwningPlayerController()->GetPlayerViewPoint(View,Look);
		for (TActorIterator<ARedNinja> It(GetWorld());It;++It)
		{
			if (It->bDefeated) continue; ++Alive;
			if (!It->bAIEnabled) continue;
			const FVector Head=It->GetActorLocation()+FVector(0,0,130);
			const FVector Screen=Canvas->Project(Head);
			FHitResult Hit;
			if (Screen.Z<=0 || Screen.X<0 || Screen.X>Canvas->ClipX || Screen.Y<0 || Screen.Y>Canvas->ClipY || FVector::Distance(View,Head)>10000 ||
				GetWorld()->LineTraceSingleByObjectType(Hit,View,Head,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionQueryParams(SCENE_QUERY_STAT(NinjaLabel),true))) continue;
			const FString State=StaticEnum<ENinjaBrainState>()->GetNameStringByValue(int64(It->BrainState));
			DrawText((It->ActorHasTag(TEXT("RoofA"))?TEXT("ROOF / "):TEXT("STREET / "))+State,FLinearColor(1,.25f,.2f),Screen.X-45*Scale,Screen.Y,nullptr,Scale);
		}
		DrawText(FString::Printf(TEXT("ENCOUNTER: %d RED NINJAS / STREET + ROOF"),Alive),FLinearColor(.85f,.3f,.2f),X,285*Scale,nullptr,Scale);
		DrawText(bCity?TEXT("Street ahead / low roof to the west"):TEXT("Use the triple jump to reach the 8m roof"),FLinearColor(.7f,.8f,.9f),X,304*Scale,nullptr,Scale);
	}
	if (Match) DrawMatchHUD(Match);
}

void ANinjaTrainingHUD::DrawMatchHUD(ANinjaMatchDirector* Match)
{
	const float W=Canvas->ClipX,H=Canvas->ClipY,S=FMath::Clamp(W/1280.f,.75f,1.5f);
	const int32 Seconds=FMath::CeilToInt(Match->RemainingSeconds);
	const FString Time=FString::Printf(TEXT("%02d:%02d"),Seconds/60,Seconds%60);
	DrawRect(FLinearColor(.015,.025,.045,.9),W-270*S,16*S,254*S,112*S);
	DrawText(Time,Seconds<=30?FLinearColor(1,.3,.2):FLinearColor::White,W-250*S,29*S,nullptr,2.6*S);
	const auto* Ninja=Cast<ANinjaCharacter>(GetOwningPawn());
	DrawText(FString::Printf(TEXT("ENEMIES %d / %d   ONE HIT"),Match->RemainingEnemies(),Match->Enemies.Num()),FLinearColor(.9,.8,.6),W-250*S,82*S,nullptr,S);
	const bool Paused=UGameplayStatics::IsGamePaused(this);
	LastMarkerCount=LastOffscreenMarkerCount=0;
	if (Match->State==ENinjaMatchState::Playing && !Paused)
	{
		DrawText(TEXT("P: pause  |  R: return"),FLinearColor::White,W-270*S,140*S,nullptr,S);
		FVector View; FRotator Look; GetOwningPlayerController()->GetPlayerViewPoint(View,Look);
		const FVector Right=FRotationMatrix(Look).GetUnitAxis(EAxis::Y),Forward=Look.Vector();
		ARedNinja* NextEnemy=nullptr;
		for (ARedNinja* Enemy:Match->Enemies) if (IsValid(Enemy) && !Enemy->bDefeated && Enemy->RouteOrder>0 && (!NextEnemy || Enemy->RouteOrder<NextEnemy->RouteOrder)) NextEnemy=Enemy;
		if (NextEnemy)
		{
			DrawRect(FLinearColor(.015,.025,.045,.9),16*S,H-102*S,610*S,86*S);
			DrawText(FString::Printf(TEXT("NEXT %02d / %d  %s"),NextEnemy->RouteOrder,Match->Enemies.Num(),*NextEnemy->RouteHint),FLinearColor(.25,1,.95),28*S,H-88*S,nullptr,1.1*S);
			DrawText(TEXT("Cyan steps: single jumps  |  Triple jump: shortcut"),FLinearColor::White,28*S,H-58*S,nullptr,S);
			DrawText(TEXT("Any defeat order is allowed. Aim at the red ninja."),FLinearColor(.7,.8,.9),28*S,H-36*S,nullptr,.9*S);
			const float FootZ=Ninja->GetActorLocation().Z-90;
			for (const FVector& Step:NextEnemy->AssistRoute) if (Step.Z>FootZ+120)
			{
				FVector Guide=Step;
				const FVector Entry=NextEnemy->AssistRoute[0];
				const bool EntryGuide=NextEnemy->AssistRoute.Num()>1 && FVector::Dist2D(Ninja->GetActorLocation(),Entry)>700 && FootZ<NextEnemy->AssistRoute[1].Z-80;
				if (EntryGuide) Guide=Entry;
				const FVector P=Canvas->Project(Guide+FVector(0,0,70));
				FVector2D Point(P.X,P.Y),Direction(0,-1);
				if (P.Z<=0 || P.X<150*S || P.X>W-150*S || P.Y<150*S || P.Y>H-150*S)
				{
					const FVector D=Guide-View;
					const double Angle=FMath::Atan2(FVector::DotProduct(D,Right),FVector::DotProduct(D,Forward));
					Direction=FVector2D(FMath::Sin(Angle),-FMath::Cos(Angle));
					const double Radius=FMath::Min((W/2-160*S)/FMath::Max(.001,FMath::Abs(Direction.X)),(H/2-155*S)/FMath::Max(.001,FMath::Abs(Direction.Y)));
					Point=FVector2D(W/2,H/2)+Direction*Radius;
				}
				DrawText(FString::Printf(TEXT("%s %.0fm"),EntryGuide?TEXT("STAIRS"):TEXT("STEP"),FVector::Dist2D(Ninja->GetActorLocation(),Guide)/100),FLinearColor(.2,1,1),Point.X,Point.Y,nullptr,S);
				DrawLine(Point.X,Point.Y,Point.X-Direction.X*12+Direction.Y*6,Point.Y-Direction.Y*12-Direction.X*6,FLinearColor(.2,1,1),2*S);
				DrawLine(Point.X,Point.Y,Point.X-Direction.X*12-Direction.Y*6,Point.Y-Direction.Y*12+Direction.X*6,FLinearColor(.2,1,1),2*S);
				break;
			}
		}
		for (ARedNinja* Enemy:Match->Enemies) if (IsValid(Enemy) && !Enemy->bDefeated)
		{
			const FVector Target=Enemy->GetActorLocation()+FVector(0,0,130),D=Target-View;
			const FVector Projected=Canvas->Project(Target);
			FVector2D Point(Projected.X,Projected.Y),Direction(0,-1);
			const bool Offscreen=Projected.Z<=0 || Point.X<70*S || Point.X>W-70*S || Point.Y<70*S || Point.Y>H-70*S;
			if (Offscreen)
			{
				const double Angle=FMath::Atan2(FVector::DotProduct(D,Right),FVector::DotProduct(D,Forward));
				Direction=FVector2D(FMath::Sin(Angle),-FMath::Cos(Angle));
				const double Radius=FMath::Min((W/2-80*S)/FMath::Max(.001,FMath::Abs(Direction.X)),(H/2-90*S)/FMath::Max(.001,FMath::Abs(Direction.Y)));
				Point=FVector2D(W/2,H/2)+Direction*Radius;
			}
			// Keep one offscreen destination readable; visible enemies retain red pins.
			if (Offscreen && NextEnemy && Enemy!=NextEnemy) continue;
			++LastMarkerCount; if (Offscreen) ++LastOffscreenMarkerCount;
			const FLinearColor Red=Enemy==NextEnemy?FLinearColor(1,.85,.15):FLinearColor(1,.25,.15); const FVector2D Side(-Direction.Y,Direction.X);
			DrawLine(Point.X,Point.Y,Point.X-Direction.X*13*S+Side.X*7*S,Point.Y-Direction.Y*13*S+Side.Y*7*S,Red,2*S);
			DrawLine(Point.X,Point.Y,Point.X-Direction.X*13*S-Side.X*7*S,Point.Y-Direction.Y*13*S-Side.Y*7*S,Red,2*S);
			const float Up=(Enemy->GetActorLocation().Z-Ninja->GetActorLocation().Z)/100;
			const FString Label=Enemy->RouteOrder>0?FString::Printf(TEXT("%02d %.0fm %+.0fm"),Enemy->RouteOrder,FVector::Dist(Ninja->GetActorLocation(),Enemy->GetActorLocation())/100,Up):FString::Printf(TEXT("NINJA %.0fm %+.0fm"),FVector::Dist(Ninja->GetActorLocation(),Enemy->GetActorLocation())/100,Up);
			DrawText(Label,Red,FMath::Clamp(Point.X-55*S,12.f,W-170*S),Point.Y+10*S,nullptr,S);
		}
		return;
	}
	// Let the final explosion read before the result panel covers the scene.
	if ((Match->State==ENinjaMatchState::Won || Match->State==ENinjaMatchState::Lost) && Match->ResultAge<2.1f) return;
	DrawRect(FLinearColor(0,0,0,.48),0,0,W,H);
	const float X=W/2-310*S,Y=H/2-180*S;
	DrawRect(FLinearColor(.02,.035,.065,.96),X,Y,620*S,360*S);
	FString Title,Line,Action; FLinearColor Color=FLinearColor::White;
	if (Paused) { Title=TEXT("PAUSED"); Line=TEXT("The timer and the city are paused."); Action=TEXT("P TO RESUME"); }
	else switch (Match->State)
	{
	case ENinjaMatchState::Loading: Title=TEXT("PREPARING CITY"); Line=Match->MusicError.IsEmpty()?TEXT("Preparing the city, safe footing and music..."):Match->MusicError; break;
	case ENinjaMatchState::Ready: Title=TEXT("IKEBUKURO NINJA"); Line=Match->UsesRoundMusic()?FString::Printf(TEXT("Defeat all %d red ninjas before the song ends."),Match->Enemies.Num()):FString::Printf(TEXT("Defeat all %d red ninjas within 5 minutes."),Match->Enemies.Num()); Action=TEXT("ENTER TO START"); break;
	case ENinjaMatchState::Countdown: Title=FString::Printf(TEXT("START IN %d"),FMath::Max(1,FMath::CeilToInt(Match->CountdownSeconds))); Line=TEXT("Dodge red shuriken. One hit ends the round."); break;
	case ENinjaMatchState::Won: Title=TEXT("MISSION COMPLETE"); Line=Match->ResultReason; Action=TEXT("ENTER TO RETRY"); Color=FLinearColor(.25,1,.6); break;
	case ENinjaMatchState::Lost: Title=TEXT("MISSION FAILED"); Line=Match->ResultReason; Action=TEXT("ENTER TO RETRY"); Color=FLinearColor(1,.3,.2); break;
	default: break;
	}
	DrawText(Title,Color,X+32*S,Y+30*S,nullptr,2.2*S);
	DrawText(Line,FLinearColor::White,X+32*S,Y+91*S,nullptr,1.1*S);
	if (Match->State==ENinjaMatchState::Won || Match->State==ENinjaMatchState::Lost)
	{
		DrawText(FString::Printf(TEXT("DEFEATED  %d / %d"),Ninja->Defeats,Match->Enemies.Num()),FLinearColor::White,X+32*S,Y+144*S,nullptr,1.3*S);
		DrawText(FString::Printf(TEXT("STOMPS  %d     SHURIKEN  %d"),Ninja->StompDefeats,Ninja->ShurikenDefeats),FLinearColor(.75,.85,.95),X+32*S,Y+181*S,nullptr,1.1*S);
		DrawText(TEXT("TIME LEFT  ")+Time,FLinearColor(.75,.85,.95),X+32*S,Y+218*S,nullptr,1.1*S);
		DrawText(Action,Color,X+32*S,Y+280*S,nullptr,1.6*S);
		return;
	}
	DrawText(TEXT("WASD move   |   MOUSE look   |   SHIFT run"),FLinearColor(.75,.85,.95),X+32*S,Y+137*S,nullptr,S);
	DrawText(TEXT("SPACE near the jump apex: up to 3 jumps"),FLinearColor(.75,.85,.95),X+32*S,Y+165*S,nullptr,S);
	DrawText(TEXT("LMB / F: throw   |   Descend onto an enemy to stomp"),FLinearColor(.75,.85,.95),X+32*S,Y+193*S,nullptr,S);
	DrawText(TEXT("Gold arrow: next enemy   |   Cyan steps: easier roof route"),FLinearColor(.75,.85,.95),X+32*S,Y+221*S,nullptr,S);
	DrawText(TEXT("Red ninja: 3-way volleys  |  One hit = game over"),FLinearColor(1,.65,.4),X+32*S,Y+249*S,nullptr,S);
	DrawText(Action,Color,X+32*S,Y+280*S,nullptr,1.6*S);
}
